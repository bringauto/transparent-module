#pragma once

#include <bringauto/transparent_module_utils/operator_stream/OperatorChannel.hpp>

#include <msquic.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

namespace bringauto::transparent_module_utils::operator_stream {

/**
 * Configuration for the operator-facing QUIC server. mTLS optional (disable client auth +
 * self-signed server cert for loopback bring-up).
 */
struct QuicOperatorServerConfig {
	std::uint16_t port{0};
	std::string alpn{"transparent-operator"};
	std::string certPath;          // server certificate (PEM)
	std::string keyPath;           // server private key (PEM)
	std::string caCertsPath;       // CA for verifying operator client certs (empty = no CA file)
	bool disableClientAuth{false}; // true for loopback/dev; false requires operator client certs
	std::string listenAddress;     // empty = any
};

/**
 * Operator-facing QUIC server living inside the Transparent module's external-server plugin —
 * the operator leg, used alongside (not instead of) the existing Fleet HTTP API path (BAF-1744;
 * ported from teleop-module's QuicOperatorServer, BAF-1670, single-operator Phase 1).
 *
 * Wire format is plain JSON (nlohmann::json), not protobuf: QUIC is just a transport, and this
 * module's whole design already treats every payload as opaque JSON text (see its README), so a
 * protobuf envelope around that JSON would be pure overhead — plus teleop-module's equivalent
 * class, which this was ported from, needs its own .proto file distinct from ours anyway (same
 * external-server-cpp process, process-global protobuf descriptor pool), a problem plain JSON
 * doesn't have at all. Envelope shape: {"kind": "hello"|"status"|"command", "device": {"module_id",
 * "type", "role", "name"}, "timestamp_ms": <int64>, "payload": <the opaque BAF-1651 JSON envelope>}.
 *
 *   cloud -> operator : sendStatus() opens a short unidirectional stream per envelope{kind:status}
 *                       (START|FIN), freeing the send buffer on SEND_COMPLETE.
 *   operator -> cloud : each operator command arrives as a unidirectional stream; the receive
 *                       callback accumulates the bytes, parses envelope{kind:command}, and hands
 *                       it to the OperatorChannel (which unblocks the module's wait_for_command()).
 *
 * Threading: msquic invokes the callbacks on its own worker thread(s). Commands cross into the
 * synchronous module API through the OperatorChannel (the thread-safe seam). sendStatus() is called
 * on the ES thread (from forward_status) and msquic StreamSend is thread-safe.
 */
class QuicOperatorServer {
public:
	QuicOperatorServer(QuicOperatorServerConfig config, OperatorChannel &channel);
	~QuicOperatorServer();

	QuicOperatorServer(const QuicOperatorServer &) = delete;
	QuicOperatorServer &operator=(const QuicOperatorServer &) = delete;

	/// Open msquic, registration, configuration, credential (mTLS), and the listener. False on any failure.
	[[nodiscard]] bool initialize();

	/// Start the listener. False on failure.
	[[nodiscard]] bool start();

	/// Stop the listener + drop the operator connection.
	void stop();

	/// Outcome of sendStatus(). NoOperator is an expected best-effort drop (nobody to send to),
	/// distinct from SendFailed (serialize / StreamOpen / StreamSend error) so the caller can react.
	enum class SendResult { Sent, NoOperator, SendFailed };

	/// Send one status to the connected operator (best-effort: NoOperator if none). Called from the
	/// ES thread. payload is the opaque status buffer (today: JSON); device_* are the fleet-protocol coords.
	SendResult sendStatus(std::uint32_t module_id, std::uint32_t device_type, const std::string &device_role,
						  const std::string &device_name, const std::uint8_t *payload, std::size_t payload_size,
						  std::int64_t timestamp_ms);

	[[nodiscard]] bool hasOperator() const { return operatorConnection_.load() != nullptr; }

private:
	/// Heap buffer kept alive across an async StreamSend; freed in the send callback on SEND_COMPLETE.
	struct SendBuffer {
		explicit SendBuffer(std::string &&data) : payload(std::move(data)) {
			buffer.Length = static_cast<std::uint32_t>(payload.size());
			buffer.Buffer = reinterpret_cast<std::uint8_t *>(payload.data());
		}
		std::string payload;
		QUIC_BUFFER buffer{};
	};

	/// Per-inbound-stream accumulation context (a command may arrive in multiple RECEIVE events).
	struct InboundStreamContext {
		QuicOperatorServer *server{nullptr};
		HQUIC connection{nullptr}; // owning connection; commands are accepted only while it is the operator
		std::string bytes;
	};

	bool loadQuic();
	bool initRegistration();
	bool initConfiguration();
	bool initCredential();
	bool initListener();

	/// Parse an accumulated command frame and hand it to the channel.
	void handleCommandBytes(const std::string &bytes);

	static QUIC_STATUS QUIC_API listenerCallback(HQUIC listener, void *context, QUIC_LISTENER_EVENT *event);
	static QUIC_STATUS QUIC_API connectionCallback(HQUIC connection, void *context, QUIC_CONNECTION_EVENT *event);
	static QUIC_STATUS QUIC_API commandStreamCallback(HQUIC stream, void *context, QUIC_STREAM_EVENT *event);
	static QUIC_STATUS QUIC_API sendStreamCallback(HQUIC stream, void *context, QUIC_STREAM_EVENT *event);

	QuicOperatorServerConfig config_;
	OperatorChannel &channel_;

	const QUIC_API_TABLE *quic_{nullptr};
	HQUIC registration_{nullptr};
	HQUIC quicConfig_{nullptr};
	HQUIC listener_{nullptr};
	QUIC_ADDR quicAddr_{};
	QUIC_BUFFER alpnBuffer_{};

	/// Single active operator connection (Phase 1: single operator).
	std::atomic<HQUIC> operatorConnection_{nullptr};
	std::atomic<bool> running_{false};

	/// Serializes use of the connection handle (sendStatus' StreamOpen/StreamSend) against its
	/// teardown (ConnectionClose in the SHUTDOWN_COMPLETE callback). The atomic above guards the
	/// pointer value only, not the lifetime of the handle it points to.
	std::mutex connectionMutex_;
};

} // namespace bringauto::transparent_module_utils::operator_stream
