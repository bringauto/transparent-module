#include <bringauto/transparent_module_utils/operator_stream/QuicOperatorServer.hpp>

#include "transparent_operator_stream.pb.h"

#include <msquicp.h> // QuicAddrSetFamily / QuicAddrSetPort helpers

#include <iostream>
#include <memory>
#include <sstream>

namespace bringauto::transparent_module_utils::operator_stream {

namespace {
constexpr std::string_view APP_NAME{"transparent-operator-server"};

// Upper bound on a single inbound command frame — small JSON; this caps the per-stream
// accumulation buffer so a peer cannot exhaust memory by streaming bytes without ever sending FIN
// (one such buffer exists per concurrent unidirectional stream).
constexpr std::size_t MAX_COMMAND_FRAME_BYTES{64U * 1024U};

// This module has no logging infrastructure of its own (unlike teleop-module's EsLogger, which
// this class is ported from) — plain stderr lines are enough for this bring-up.
void logInfo(const std::string &msg) { std::cerr << "[transparent-quic] INFO: " << msg << std::endl; }
void logWarning(const std::string &msg) { std::cerr << "[transparent-quic] WARN: " << msg << std::endl; }
void logError(const std::string &msg) { std::cerr << "[transparent-quic] ERROR: " << msg << std::endl; }

std::string toHex(unsigned value) {
	std::ostringstream oss;
	oss << std::hex << value;
	return oss.str();
}

// Append every QUIC_BUFFER segment of a RECEIVE event onto the per-stream accumulator.
void appendBuffers(std::string &out, const QUIC_BUFFER *buffers, std::uint32_t bufferCount, std::uint64_t totalLength) {
	out.reserve(out.size() + totalLength);
	for (std::uint32_t i = 0; i < bufferCount; ++i) {
		out.append(reinterpret_cast<const char *>(buffers[i].Buffer), buffers[i].Length);
	}
}
} // namespace

QuicOperatorServer::QuicOperatorServer(QuicOperatorServerConfig config, OperatorChannel &channel)
	: config_(std::move(config)), channel_(channel) {
	alpnBuffer_ = {static_cast<std::uint32_t>(config_.alpn.size()),
				   reinterpret_cast<std::uint8_t *>(config_.alpn.data())};
}

QuicOperatorServer::~QuicOperatorServer() {
	stop();
	if (quicConfig_ != nullptr) {
		quic_->ConfigurationClose(quicConfig_);
	}
	if (registration_ != nullptr) {
		quic_->RegistrationClose(registration_);
	}
	if (quic_ != nullptr) {
		MsQuicClose(quic_);
	}
}

QuicOperatorServer::InitResult QuicOperatorServer::initialize() {
	const bool ok = loadQuic() && initRegistration() && initConfiguration() && initCredential() && initListener();
	return ok ? InitResult::Ok : InitResult::Failed;
}

bool QuicOperatorServer::loadQuic() {
	const QUIC_STATUS status = MsQuicOpen2(&quic_);
	if (QUIC_FAILED(status)) {
		logError("QUIC operator server: MsQuicOpen2 failed, status=" + std::to_string(static_cast<unsigned>(status)));
		return false;
	}
	return true;
}

bool QuicOperatorServer::initRegistration() {
	QUIC_REGISTRATION_CONFIG config{};
	config.AppName = APP_NAME.data();
	config.ExecutionProfile = QUIC_EXECUTION_PROFILE_LOW_LATENCY;
	const QUIC_STATUS status = quic_->RegistrationOpen(&config, &registration_);
	if (QUIC_FAILED(status)) {
		logError("QUIC operator server: RegistrationOpen failed, status=" +
				 std::to_string(static_cast<unsigned>(status)));
		return false;
	}
	return true;
}

bool QuicOperatorServer::initConfiguration() {
	QUIC_SETTINGS settings{};
	settings.IsSet.IdleTimeoutMs = TRUE;
	settings.IdleTimeoutMs = 30000;
	settings.IsSet.KeepAliveIntervalMs = TRUE;
	settings.KeepAliveIntervalMs = 5000;
	// The operator opens unidirectional streams to send commands (one per command); allow plenty.
	settings.IsSet.PeerUnidiStreamCount = TRUE;
	settings.PeerUnidiStreamCount = 1024;
	settings.IsSet.SendBufferingEnabled = TRUE;
	settings.SendBufferingEnabled = TRUE;
	settings.IsSet.ServerResumptionLevel = TRUE;
	settings.ServerResumptionLevel = QUIC_SERVER_RESUME_AND_ZERORTT;

	const QUIC_STATUS status =
			quic_->ConfigurationOpen(registration_, &alpnBuffer_, 1, &settings, sizeof(settings), nullptr, &quicConfig_);
	if (QUIC_FAILED(status)) {
		logError("QUIC operator server: ConfigurationOpen failed, status=" +
				 std::to_string(static_cast<unsigned>(status)));
		return false;
	}
	return true;
}

bool QuicOperatorServer::initCredential() {
	QUIC_CERTIFICATE_FILE certificateFile{};
	certificateFile.CertificateFile = config_.certPath.c_str();
	certificateFile.PrivateKeyFile = config_.keyPath.c_str();

	QUIC_CREDENTIAL_CONFIG credential{};
	credential.Type = QUIC_CREDENTIAL_TYPE_CERTIFICATE_FILE;
	credential.CertificateFile = &certificateFile;
	if (!config_.caCertsPath.empty()) {
		credential.Flags |= QUIC_CREDENTIAL_FLAG_SET_CA_CERTIFICATE_FILE;
		credential.CaCertificateFile = config_.caCertsPath.c_str();
	}
	if (config_.disableClientAuth) {
		logWarning("QUIC operator server: client authentication DISABLED (dev/loopback only)");
	} else {
		credential.Flags |= QUIC_CREDENTIAL_FLAG_REQUIRE_CLIENT_AUTHENTICATION;
	}

	const QUIC_STATUS status = quic_->ConfigurationLoadCredential(quicConfig_, &credential);
	if (QUIC_FAILED(status)) {
		logError("QUIC operator server: ConfigurationLoadCredential failed, status=" +
				 std::to_string(static_cast<unsigned>(status)));
		return false;
	}
	return true;
}

bool QuicOperatorServer::initListener() {
	QUIC_STATUS status = quic_->ListenerOpen(registration_, listenerCallback, this, &listener_);
	if (QUIC_FAILED(status)) {
		logError("QUIC operator server: ListenerOpen failed, status=" +
				 std::to_string(static_cast<unsigned>(status)));
		return false;
	}
	if (!config_.listenAddress.empty()) {
		// Bind the configured address (IPv4 or IPv6); QuicAddrFromString also sets the port.
		if (!QuicAddrFromString(config_.listenAddress.c_str(), config_.port, &quicAddr_)) {
			logError("QUIC operator server: invalid quic_listen_address '" + config_.listenAddress + "'");
			quic_->ListenerClose(listener_);
			listener_ = nullptr;
			return false;
		}
	} else {
		// No address configured: bind any interface, dual-stack (IPv4 + IPv6).
		QuicAddrSetFamily(&quicAddr_, QUIC_ADDRESS_FAMILY_UNSPEC);
		QuicAddrSetPort(&quicAddr_, config_.port);
	}
	return true;
}

QuicOperatorServer::InitResult QuicOperatorServer::start() {
	const QUIC_STATUS status = quic_->ListenerStart(listener_, &alpnBuffer_, 1, &quicAddr_);
	if (QUIC_FAILED(status)) {
		logError("QUIC operator server: ListenerStart failed, status=" +
				 std::to_string(static_cast<unsigned>(status)));
		// The listener was opened but never started, so running_ stays false and stop() will not tear it
		// down — close it here. Otherwise the handle leaks and RegistrationClose in the destructor can
		// block on the still-open listener (e.g. when the port is already in use).
		quic_->ListenerClose(listener_);
		listener_ = nullptr;
		return InitResult::Failed;
	}
	running_.store(true);
	logInfo("QUIC operator server: listening on port " + std::to_string(config_.port) + " (alpn=" + config_.alpn + ")");
	return InitResult::Ok;
}

void QuicOperatorServer::stop() {
	if (!running_.exchange(false)) {
		return;
	}
	if (listener_ != nullptr) {
		quic_->ListenerStop(listener_);
	}
	HQUIC conn = operatorConnection_.exchange(nullptr);
	if (conn != nullptr) {
		quic_->ConnectionShutdown(conn, QUIC_CONNECTION_SHUTDOWN_FLAG_NONE, 0);
	}
	channel_.shutdown();
}

QuicOperatorServer::SendResult QuicOperatorServer::sendStatus(std::uint32_t module_id, std::uint32_t device_type,
															 std::string_view device_role, std::string_view device_name,
															 std::span<const std::uint8_t> payload,
															 std::int64_t timestamp_ms) {
	if (operatorConnection_.load() == nullptr) {
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

	// Hold connectionMutex_ across the whole handle use so the SHUTDOWN_COMPLETE callback cannot
	// ConnectionClose the handle while we are opening/sending on it. Re-load under the lock: the
	// connection may have been torn down between the early-out check above and here.
	std::lock_guard<std::mutex> lock(connectionMutex_);
	HQUIC connection = operatorConnection_.load();
	if (connection == nullptr) {
		return SendResult::NoOperator; // operator disconnected while we were serializing — drop
	}

	HQUIC stream{nullptr};
	QUIC_STATUS status_rc =
			quic_->StreamOpen(connection, QUIC_STREAM_OPEN_FLAG_UNIDIRECTIONAL, sendStreamCallback, this, &stream);
	if (QUIC_FAILED(status_rc)) {
		logError("QUIC operator server: StreamOpen (status) failed, status=" +
				 std::to_string(static_cast<unsigned>(status_rc)));
		return SendResult::SendFailed;
	}
	auto sendBuffer = std::make_unique<SendBuffer>(std::move(serialized));
	status_rc = quic_->StreamSend(stream, &sendBuffer->buffer, 1, QUIC_SEND_FLAG_START | QUIC_SEND_FLAG_FIN,
								  sendBuffer.get());
	if (QUIC_FAILED(status_rc)) {
		logError("QUIC operator server: StreamSend (status) failed, status=" +
				 std::to_string(static_cast<unsigned>(status_rc)));
		// sendBuffer is freed automatically on return. The stream was opened but never started (START
		// rode on the failed send), so no sendStreamCallback will fire to close it — close it here to
		// avoid leaking the handle.
		quic_->StreamClose(stream);
		return SendResult::SendFailed;
	}
	// Ownership transfers to msquic; sendStreamCallback reclaims it on SEND_COMPLETE.
	sendBuffer.release();
	return SendResult::Sent;
}

void QuicOperatorServer::handleCommandBytes(const std::string &bytes) {
	bringauto::transparent::operator_stream::OperatorMessage message;
	if (!message.ParseFromString(bytes)) {
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

QUIC_STATUS QUIC_API QuicOperatorServer::listenerCallback(HQUIC listener, void *context, QUIC_LISTENER_EVENT *event) {
	auto *self = static_cast<QuicOperatorServer *>(context);
	switch (event->Type) {
		case QUIC_LISTENER_EVENT_NEW_CONNECTION:
			self->quic_->SetCallbackHandler(event->NEW_CONNECTION.Connection,
											reinterpret_cast<void *>(connectionCallback), self);
			return self->quic_->ConnectionSetConfiguration(event->NEW_CONNECTION.Connection, self->quicConfig_);
		case QUIC_LISTENER_EVENT_STOP_COMPLETE:
			if (!event->STOP_COMPLETE.AppCloseInProgress) {
				self->quic_->ListenerClose(listener);
			}
			self->listener_ = nullptr;
			return QUIC_STATUS_SUCCESS;
		default:
			return QUIC_STATUS_NOT_SUPPORTED;
	}
}

QUIC_STATUS QUIC_API QuicOperatorServer::connectionCallback(HQUIC connection, void *context,
														   QUIC_CONNECTION_EVENT *event) {
	auto *self = static_cast<QuicOperatorServer *>(context);
	switch (event->Type) {
		case QUIC_CONNECTION_EVENT_CONNECTED: {
			HQUIC expected = nullptr;
			if (!self->operatorConnection_.compare_exchange_strong(expected, connection)) {
				logWarning("QUIC operator server: an operator is already connected, rejecting new one "
						   "(single-operator)");
				self->quic_->ConnectionShutdown(connection, QUIC_CONNECTION_SHUTDOWN_FLAG_NONE, 0);
				return QUIC_STATUS_SUCCESS;
			}
			logInfo("QUIC operator server: operator connected");
			return QUIC_STATUS_SUCCESS;
		}
		case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_TRANSPORT: {
			logInfo("QUIC operator server: SHUTDOWN_INITIATED_BY_TRANSPORT, status=0x" +
					toHex(static_cast<unsigned>(event->SHUTDOWN_INITIATED_BY_TRANSPORT.Status)));
			HQUIC expected = connection;
			if (self->operatorConnection_.compare_exchange_strong(expected, nullptr)) {
				self->channel_.clear();
				logInfo("QUIC operator server: operator disconnected");
			} else {
				logWarning("QUIC operator server: SHUTDOWN_INITIATED_BY_TRANSPORT for a connection that was not "
						   "the tracked operator (already replaced/cleared?)");
			}
			return QUIC_STATUS_SUCCESS;
		}
		case QUIC_CONNECTION_EVENT_SHUTDOWN_INITIATED_BY_PEER: {
			logInfo("QUIC operator server: SHUTDOWN_INITIATED_BY_PEER, error_code=" +
					std::to_string(event->SHUTDOWN_INITIATED_BY_PEER.ErrorCode));
			HQUIC expected = connection;
			if (self->operatorConnection_.compare_exchange_strong(expected, nullptr)) {
				self->channel_.clear();
				logInfo("QUIC operator server: operator disconnected");
			} else {
				logWarning("QUIC operator server: SHUTDOWN_INITIATED_BY_PEER for a connection that was not "
						   "the tracked operator (already replaced/cleared?)");
			}
			return QUIC_STATUS_SUCCESS;
		}
		case QUIC_CONNECTION_EVENT_PEER_STREAM_STARTED: {
			// Only the accepted operator may feed the command channel. A stream opened by any other
			// connection (a second operator being rejected, or one mid-shutdown) is rejected so its
			// bytes never reach the device. Phase 1 is single-operator.
			// Every stream surfaced here — accepted or rejected — gets a callback handler and is
			// routed through commandStreamCallback, which already drops bytes for a connection that
			// isn't the tracked operator. A rejected stream is aborted via StreamShutdown instead of
			// being closed immediately: msquic requires a stream to be fully shut down
			// (SHUTDOWN_COMPLETE) before StreamClose, and closing before that is undefined behavior —
			// commandStreamCallback's SHUTDOWN_COMPLETE case performs the actual StreamClose.
			auto streamCtx = std::make_unique<InboundStreamContext>(InboundStreamContext{self, connection, {}});
			self->quic_->SetCallbackHandler(event->PEER_STREAM_STARTED.Stream,
											reinterpret_cast<void *>(commandStreamCallback), streamCtx.get());
			// Ownership transfers to commandStreamCallback; reclaimed on SHUTDOWN_COMPLETE.
			streamCtx.release();
			if (connection != self->operatorConnection_.load()) {
				self->quic_->StreamShutdown(event->PEER_STREAM_STARTED.Stream, QUIC_STREAM_SHUTDOWN_FLAG_ABORT, 0);
			}
			return QUIC_STATUS_SUCCESS;
		}
		case QUIC_CONNECTION_EVENT_SHUTDOWN_COMPLETE:
			logInfo("QUIC operator server: SHUTDOWN_COMPLETE, AppCloseInProgress=" +
					std::to_string(static_cast<int>(event->SHUTDOWN_COMPLETE.AppCloseInProgress)));
			if (!event->SHUTDOWN_COMPLETE.AppCloseInProgress) {
				// Serialize against sendStatus: it may still be inside StreamOpen/StreamSend on this
				// handle. Closing it concurrently would be a use-after-free.
				std::lock_guard<std::mutex> lock(self->connectionMutex_);
				self->quic_->ConnectionClose(connection);
			}
			return QUIC_STATUS_SUCCESS;
		default:
			return QUIC_STATUS_SUCCESS;
	}
}

QUIC_STATUS QUIC_API QuicOperatorServer::commandStreamCallback(HQUIC stream, void *context, QUIC_STREAM_EVENT *event) {
	auto *ctx = static_cast<InboundStreamContext *>(context);
	switch (event->Type) {
		case QUIC_STREAM_EVENT_RECEIVE: {
			if (event->RECEIVE.BufferCount > 0) {
				appendBuffers(ctx->bytes, event->RECEIVE.Buffers, event->RECEIVE.BufferCount,
							  event->RECEIVE.TotalBufferLength);
			}
			if (ctx->bytes.size() > MAX_COMMAND_FRAME_BYTES) {
				logWarning("QUIC operator server: inbound command frame exceeded " +
						   std::to_string(MAX_COMMAND_FRAME_BYTES) + " bytes, aborting stream");
				ctx->bytes.clear();
				ctx->server->quic_->StreamShutdown(stream, QUIC_STREAM_SHUTDOWN_FLAG_ABORT, 0);
				return QUIC_STATUS_SUCCESS;
			}
			if (event->RECEIVE.Flags & QUIC_RECEIVE_FLAG_FIN) {
				if (ctx->connection == ctx->server->operatorConnection_.load()) {
					ctx->server->handleCommandBytes(ctx->bytes);
				}
				ctx->bytes.clear();
			}
			return QUIC_STATUS_SUCCESS;
		}
		case QUIC_STREAM_EVENT_PEER_SEND_SHUTDOWN:
			if (!ctx->bytes.empty()) {
				if (ctx->connection == ctx->server->operatorConnection_.load()) {
					ctx->server->handleCommandBytes(ctx->bytes);
				}
				ctx->bytes.clear();
			}
			return QUIC_STATUS_SUCCESS;
		case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE: {
			// Reclaim the context whose ownership PEER_STREAM_STARTED handed to msquic; freed at scope end.
			std::unique_ptr<InboundStreamContext> owned{ctx};
			if (!event->SHUTDOWN_COMPLETE.AppCloseInProgress) {
				ctx->server->quic_->StreamClose(stream);
			}
			return QUIC_STATUS_SUCCESS;
		}
		default:
			return QUIC_STATUS_SUCCESS;
	}
}

QUIC_STATUS QUIC_API QuicOperatorServer::sendStreamCallback(HQUIC stream, void *context, QUIC_STREAM_EVENT *event) {
	auto *self = static_cast<QuicOperatorServer *>(context);
	switch (event->Type) {
		case QUIC_STREAM_EVENT_SEND_COMPLETE:
			// Reclaim the buffer whose ownership sendStatus handed to msquic; freed at end of statement.
			std::unique_ptr<SendBuffer>{static_cast<SendBuffer *>(event->SEND_COMPLETE.ClientContext)};
			return QUIC_STATUS_SUCCESS;
		case QUIC_STREAM_EVENT_SHUTDOWN_COMPLETE:
			if (!event->SHUTDOWN_COMPLETE.AppCloseInProgress) {
				self->quic_->StreamClose(stream);
			}
			return QUIC_STATUS_SUCCESS;
		default:
			return QUIC_STATUS_SUCCESS;
	}
}

} // namespace bringauto::transparent_module_utils::operator_stream
