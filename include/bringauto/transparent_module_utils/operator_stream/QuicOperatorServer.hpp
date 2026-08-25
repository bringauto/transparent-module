#pragma once

#include <bringauto/transparent_module_utils/operator_stream/OperatorChannel.hpp>

#include <bringauto/quic/QuicServer.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>

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
 * the operator leg, taking over from the existing Fleet HTTP API path when quic_port is
 * configured (see external_server_api.cpp); ported from teleop-module's QuicOperatorServer,
 * single-operator Phase 1.
 *
 * Sits on top of the shared, transport-only `bringauto::quic::QuicServer` (ba-quic-lib) instead of
 * raw msquic, mirroring teleop-module's own migration. This class now owns only the
 * operator-protocol-specific pieces above the transport boundary — OperatorMessage
 * (de)serialization, and tracking which tracked connection is currently "the operator". Public API
 * is unchanged, so external_server_api.cpp needs no changes.
 *
 * Wire format is protobuf (proto/transparent_operator_stream.proto's OperatorMessage), matching
 * teleop-module's QuicOperatorServer this was ported from — but its own .proto/package, since
 * external-server-cpp dlopens both teleop-external-server-shared.so and
 * transparent-external-server-shared.so into the same process, and protobuf's generated-descriptor
 * registry is process-global: two .so's registering the same .proto file/package would crash. The
 * streaming-control JSON envelope itself still rides as opaque `bytes` inside OperatorMessage's
 * StatusUpdate/CommandRequest — this class stays decoupled from that schema.
 *
 * Single operator: `quicServer_.maxConnections = 1` rejects a second concurrent connect attempt at
 * the transport layer already (ConnectionShutdown), so — unlike the old hand-rolled version — this
 * class no longer needs its own compare-and-swap "already have an operator" logic.
 *
 * BAF-1900 update: the above does not hold for two handshakes reaching CONNECTED simultaneously —
 * ba-quic-lib's own `QuicServer::onConnected` documents this race as fail-open. `onConnected()`
 * does guard `operatorConnection_` with an explicit check-and-set after all, refusing
 * (`disconnect()`) any additional operator once one is set (mirrors teleop-module's
 * `QuicOperatorServer`).
 */
class QuicOperatorServer {
public:
	/// Construct with the given config and channel; does not start listening (see start()).
	QuicOperatorServer(QuicOperatorServerConfig config, OperatorChannel &channel);
	/// Stops the listener and drops any operator connection if still running.
	~QuicOperatorServer();

	QuicOperatorServer(const QuicOperatorServer &) = delete;
	QuicOperatorServer &operator=(const QuicOperatorServer &) = delete;

	/// Outcome of initialize()/start().
	enum class InitResult { Ok, Failed };

	/// Initialize the underlying ba-quic-lib transport (registration, TLS credential, listener
	/// setup). Does not begin accepting connections; see start().
	[[nodiscard]] InitResult initialize();

	/// Start the listener.
	[[nodiscard]] InitResult start();

	/// Stop the listener + drop the operator connection.
	void stop();

	/// Outcome of sendStatus(). NoOperator is an expected best-effort drop (nobody to send to),
	/// distinct from SendFailed (serialize / transport send error) so the caller can react.
	enum class SendResult { Sent, NoOperator, SendFailed };

	/// Send one status to the connected operator (best-effort: NoOperator if none). Called from the
	/// ES thread. payload is the opaque status buffer (today: JSON); device_* are the fleet-protocol coords.
	SendResult sendStatus(std::uint32_t module_id, std::uint32_t device_type, std::string_view device_role,
						  std::string_view device_name, std::span<const std::uint8_t> payload,
						  std::int64_t timestamp_ms);

	/// True while an operator is currently connected.
	[[nodiscard]] bool hasOperator() const;

private:
	using ConnectionId = bringauto::quic::QuicServer::ConnectionId;

	[[nodiscard]] bringauto::quic::QuicEndpointConfig buildEndpointConfig() const;
	[[nodiscard]] bringauto::quic::QuicSettings buildSettings() const;

	/// Fired once per accepted connection (already filtered by maxConnections=1 below this class).
	void onConnected(ConnectionId id);
	/// Fired once per terminal disconnect of a connection that was previously reported via onConnected.
	void onDisconnected(ConnectionId id);
	/// Fired once per fully-reassembled inbound frame. Parses it as an OperatorMessage and, if it
	/// carries a command, hands it to the OperatorChannel.
	void onBytesReceived(ConnectionId id, std::vector<std::uint8_t> bytes);

	QuicOperatorServerConfig config_;
	OperatorChannel &channel_;

	/// Guards operatorConnection_. Single operator (Phase 1, same as teleop-module).
	mutable std::mutex operatorMutex_;
	std::optional<ConnectionId> operatorConnection_;

	/// Makes stop() idempotent — an explicit stop() followed by the destructor's stop() call
	/// must not run quicServer_->stop()/channel_.shutdown() twice.
	std::atomic<bool> running_{true};

	/// The shared, transport-only ba-quic-lib server. Declared last so it is destroyed first —
	/// its callbacks, still possibly in flight during destruction, read/write operatorMutex_ and
	/// operatorConnection_ above, which must therefore outlive it.
	std::unique_ptr<bringauto::quic::QuicServer> quicServer_;
};

} // namespace bringauto::transparent_module_utils::operator_stream
