#include <bringauto/transparent_module_utils/operator_stream/QuicOperatorServer.hpp>

#include <msquicp.h> // QuicAddrSetFamily / QuicAddrSetPort helpers

#include <nlohmann/json.hpp>

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
// this class is ported from) — plain stderr lines are enough for this bring-up (BAF-1744).
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

bool QuicOperatorServer::initialize() {
	return loadQuic() && initRegistration() && initConfiguration() && initCredential() && initListener();
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

bool QuicOperatorServer::start() {
	const QUIC_STATUS status = quic_->ListenerStart(listener_, &alpnBuffer_, 1, &quicAddr_);
	if (QUIC_FAILED(status)) {
		logError("QUIC operator server: ListenerStart failed, status=" +
				 std::to_string(static_cast<unsigned>(status)));
		// The listener was opened but never started, so running_ stays false and stop() will not tear it
		// down — close it here. Otherwise the handle leaks and RegistrationClose in the destructor can
		// block on the still-open listener (e.g. when the port is already in use).
		quic_->ListenerClose(listener_);
		listener_ = nullptr;
		return false;
	}
	running_.store(true);
	logInfo("QUIC operator server: listening on port " + std::to_string(config_.port) + " (alpn=" + config_.alpn + ")");
	return true;
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
															 const std::string &device_role, const std::string &device_name,
															 const std::uint8_t *payload, std::size_t payload_size,
															 std::int64_t timestamp_ms) {
	if (operatorConnection_.load() == nullptr) {
		return SendResult::NoOperator; // no operator connected — drop (best-effort)
	}

	// payload is opaque bytes that happen to already be JSON text almost always (see the class doc
	// comment) — embed it as a nested JSON value so the wire form has no extra string-escaping
	// layer, falling back to a plain string if it genuinely isn't valid JSON.
	nlohmann::json envelope;
	envelope["kind"] = "status";
	envelope["device"] = {{"module_id", module_id}, {"type", device_type}, {"role", device_role}, {"name", device_name}};
	envelope["timestamp_ms"] = timestamp_ms;
	try {
		envelope["payload"] = nlohmann::json::parse(payload, payload + payload_size);
	} catch (const nlohmann::json::exception &) {
		envelope["payload"] = std::string(reinterpret_cast<const char *>(payload), payload_size);
	}

	std::string serialized;
	try {
		// replace (not the default strict): the non-JSON fallback above can embed arbitrary bytes as
		// a JSON string, and strict dump() throws on invalid UTF-8 in that string, dropping the whole
		// status. replace swaps invalid sequences for U+FFFD instead of failing the send.
		serialized = envelope.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
	} catch (const nlohmann::json::exception &e) {
		logError(std::string("QUIC operator server: failed to serialize status, dropping: ") + e.what());
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
	nlohmann::json envelope;
	try {
		envelope = nlohmann::json::parse(bytes);
	} catch (const nlohmann::json::exception &e) {
		logWarning("QUIC operator server: failed to parse command JSON (" + std::to_string(bytes.size()) +
				   " bytes): " + e.what());
		return;
	}
	if (!envelope.is_object() || envelope.value("kind", std::string{}) != "command") {
		// Malformed, or "hello" — ignored for now (single operator, always active).
		return;
	}

	const auto deviceIt = envelope.find("device");
	const nlohmann::json &device = deviceIt != envelope.end() ? *deviceIt : nlohmann::json::object();

	OperatorCommand out;
	out.timestamp_ms = envelope.value("timestamp_ms", static_cast<std::int64_t>(0));
	out.module_id = device.value("module_id", 0u);
	out.device_type = device.value("type", 0u);
	out.device_role = device.value("role", std::string{});
	out.device_name = device.value("name", std::string{});
	// The BAF-1651 envelope was embedded as a nested JSON value by the sender — re-serialize it
	// back to bytes here, since OperatorCommand::payload/downstream consumers (pop_command(),
	// rtsp-server's onFleetCommand()) all expect a flat JSON string, not a parsed value.
	const std::string payloadStr = envelope.contains("payload") ? envelope["payload"].dump() : std::string("{}");
	out.payload.assign(payloadStr.begin(), payloadStr.end());
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
			// msquic requires every stream surfaced in PEER_STREAM_STARTED to be either accepted
			// (SetCallbackHandler) or closed (StreamClose); without a registered handler there is no
			// SHUTDOWN_COMPLETE to reclaim on, so StreamShutdown alone would leak the handle. StreamClose
			// rejects it cleanly.
			if (connection != self->operatorConnection_.load()) {
				self->quic_->StreamClose(event->PEER_STREAM_STARTED.Stream);
				return QUIC_STATUS_SUCCESS;
			}
			auto streamCtx = std::make_unique<InboundStreamContext>(InboundStreamContext{self, connection, {}});
			self->quic_->SetCallbackHandler(event->PEER_STREAM_STARTED.Stream,
											reinterpret_cast<void *>(commandStreamCallback), streamCtx.get());
			// Ownership transfers to commandStreamCallback; reclaimed on SHUTDOWN_COMPLETE.
			streamCtx.release();
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
