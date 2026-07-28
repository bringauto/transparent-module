#include <bringauto/transparent_module_utils/operator_stream/QuicOperatorServer.hpp>

#include "transparent_operator_stream.pb.h"

#include <iostream>
#include <utility>

namespace bringauto::transparent_module_utils::operator_stream {

namespace {
// This module has no logging infrastructure of its own (unlike teleop-module's EsLogger, which
// this class is ported from) — plain stderr lines are enough for this bring-up.
void logInfo(const std::string &msg) { std::cerr << "[transparent-quic] INFO: " << msg << std::endl; }
void logWarning(const std::string &msg) { std::cerr << "[transparent-quic] WARN: " << msg << std::endl; }
void logError(const std::string &msg) { std::cerr << "[transparent-quic] ERROR: " << msg << std::endl; }
} // namespace

QuicOperatorServer::QuicOperatorServer(QuicOperatorServerConfig config, OperatorChannel &channel)
	: config_(std::move(config)), channel_(channel) {
	bringauto::quic::QuicServerCallbacks callbacks;
	callbacks.onConnected = [this](ConnectionId id) { onConnected(std::move(id)); };
	callbacks.onDisconnected = [this](ConnectionId id) { onDisconnected(std::move(id)); };
	callbacks.onBytesReceived = [this](ConnectionId id, std::vector<std::uint8_t> bytes) {
		onBytesReceived(std::move(id), std::move(bytes));
	};

	quicServer_ = std::make_unique<bringauto::quic::QuicServer>(buildEndpointConfig(), buildSettings(),
																 std::move(callbacks));
	// Single operator: ba-quic-lib's QuicServer::onConnected() checks maxConnections and calls
	// ConnectionShutdown *before* invoking callbacks_.onConnected() for a rejected attempt (confirmed
	// against quic-lib's QuicServer.cpp) -- so onConnected() below is never called for a connection
	// that loses the race, and this class no longer needs its own compare-and-swap "already have an
	// operator" logic.
	quicServer_->maxConnections = 1;
}

QuicOperatorServer::~QuicOperatorServer() {
	stop();
}

bringauto::quic::QuicEndpointConfig QuicOperatorServer::buildEndpointConfig() const {
	bringauto::quic::QuicEndpointConfig config;
	config.listenAddress = config_.listenAddress;
	config.port = config_.port;
	config.alpn = config_.alpn;
	config.certPath = config_.certPath;
	config.keyPath = config_.keyPath;
	config.caCertsPath = config_.caCertsPath;
	config.disableClientAuth = config_.disableClientAuth;
	return config;
}

bringauto::quic::QuicSettings QuicOperatorServer::buildSettings() const {
	bringauto::quic::QuicSettings settings;
	// Matches the old hand-rolled defaults, which differ from ba-quic-lib's own built-in defaults
	// (idleTimeoutMs=5000, keepAliveIntervalMs=1000). peerUnidiStreamCount's built-in default
	// (1024) already matches the old PeerUnidiStreamCount setting, so it is left unset here.
	settings.idleTimeoutMs = 30000;
	settings.keepAliveIntervalMs = 5000;
	settings.sendBufferingEnabled = true;
	// 2 == QUIC_SERVER_RESUME_AND_ZERORTT (msquic.h). Spelled out as a literal rather than pulling in
	// <msquic.h> for one enum value that ba-quic-lib doesn't itself expose — ba-quic-lib links
	// msquic PUBLIC only incidentally, so relying on the transitive include is fragile.
	settings.serverResumptionLevel = 2;
	return settings;
}

QuicOperatorServer::InitResult QuicOperatorServer::initialize() {
	return quicServer_->initialize() ? InitResult::Ok : InitResult::Failed;
}

QuicOperatorServer::InitResult QuicOperatorServer::start() {
	return quicServer_->start() ? InitResult::Ok : InitResult::Failed;
}

void QuicOperatorServer::stop() {
	quicServer_->stop();
	channel_.shutdown();
}

bool QuicOperatorServer::hasOperator() const {
	std::lock_guard<std::mutex> lock(operatorMutex_);
	return operatorConnection_.has_value();
}

QuicOperatorServer::SendResult QuicOperatorServer::sendStatus(std::uint32_t module_id, std::uint32_t device_type,
															 std::string_view device_role, std::string_view device_name,
															 std::span<const std::uint8_t> payload,
															 std::int64_t timestamp_ms) {
	std::optional<ConnectionId> connection;
	{
		std::lock_guard<std::mutex> lock(operatorMutex_);
		connection = operatorConnection_;
	}
	if (!connection.has_value()) {
		return SendResult::NoOperator; // no operator connected — drop (best-effort)
	}

	bringauto::transparent::operator_stream::OperatorMessage message;
	auto *status = message.mutable_status();
	auto *device = status->mutable_device();
	device->set_module_id(module_id);
	device->set_type(device_type);
	device->set_role(device_role.data(), device_role.size());
	device->set_name(device_name.data(), device_name.size());
	status->set_payload(payload.data(), payload.size());
	status->set_timestamp_ms(timestamp_ms);

	std::string serialized;
	if (!message.SerializeToString(&serialized)) {
		logError("QUIC operator server: failed to serialize status, dropping");
		return SendResult::SendFailed;
	}

	const std::vector<std::uint8_t> bytes(serialized.begin(), serialized.end());
	if (!quicServer_->send(*connection, bytes)) {
		// Covers both a synchronous transport failure and "operator disconnected between the check
		// above and here" — the caller only distinguishes "nobody to send to" as a fast pre-check.
		return SendResult::SendFailed;
	}
	return SendResult::Sent;
}

void QuicOperatorServer::onConnected(ConnectionId id) {
	std::lock_guard<std::mutex> lock(operatorMutex_);
	operatorConnection_ = std::move(id);
	logInfo("QUIC operator server: operator connected");
}

void QuicOperatorServer::onDisconnected(ConnectionId id) {
	{
		std::lock_guard<std::mutex> lock(operatorMutex_);
		if (operatorConnection_ != id) {
			return; // a connection rejected by maxConnections never became "the operator"
		}
		operatorConnection_.reset();
	}
	channel_.clear();
	logInfo("QUIC operator server: operator disconnected");
}

void QuicOperatorServer::onBytesReceived(ConnectionId id, std::vector<std::uint8_t> bytes) {
	{
		std::lock_guard<std::mutex> lock(operatorMutex_);
		if (operatorConnection_ != id) {
			return; // stale bytes from a connection no longer considered the operator
		}
	}

	bringauto::transparent::operator_stream::OperatorMessage message;
	if (!message.ParseFromArray(bytes.data(), static_cast<int>(bytes.size()))) {
		logWarning("QUIC operator server: failed to parse OperatorMessage (" + std::to_string(bytes.size()) +
				   " bytes), dropping");
		return;
	}
	if (!message.has_command()) {
		// Malformed, or Hello — ignored for now (single operator, always active).
		return;
	}
	const auto &command = message.command();
	OperatorCommand out;
	const auto &payload = command.payload();
	out.payload.assign(payload.begin(), payload.end());
	out.timestamp_ms = command.timestamp_ms();
	out.module_id = command.device().module_id();
	out.device_type = command.device().type();
	out.device_role = command.device().role();
	out.device_name = command.device().name();
	channel_.push(std::move(out));
}

} // namespace bringauto::transparent_module_utils::operator_stream
