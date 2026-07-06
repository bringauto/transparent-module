#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace bringauto::transparent_module_utils::operator_stream {

/**
 * One command received from an operator over QUIC, handed across the thread boundary to the
 * synchronous ES API. `payload` is the opaque command buffer (today: the BAF-1651 JSON envelope)
 * passed through verbatim; device_* are the fleet-protocol device coordinates kept for routing.
 *
 * Ported from teleop-module's OperatorChannel (BAF-1670) for the Transparent module's QUIC
 * operator transport (BAF-1744) — see that repo's equivalent file for the original design notes.
 */
struct OperatorCommand {
	std::vector<std::uint8_t> payload;
	std::int64_t timestamp_ms{0};
	std::uint32_t module_id{0};
	std::uint32_t device_type{0};
	std::string device_role;
	std::string device_name;
};

/**
 * Thread-safe seam between the QUIC server's ASYNCHRONOUS msquic receive callback (producer) and
 * the module's SYNCHRONOUS, blocking external-server API (consumer) — see
 * QuicOperatorServer for the producer side and external_server_api.cpp's wait_for_command() for
 * the consumer side.
 */
class OperatorChannel {
public:
	/// Producer (QUIC recv-callback thread): hand over a command and wake a waiter. Coalesces to
	/// the newest — a pending unconsumed command is dropped (freshest wins). No-op after shutdown().
	void push(OperatorCommand command);

	/// Consumer (ES `wait_for_command` thread): block up to `timeout` for the latest command.
	/// Returns std::nullopt on timeout or after shutdown().
	[[nodiscard]] std::optional<OperatorCommand> waitForCommand(std::chrono::milliseconds timeout);

	/// Drop queued commands — e.g. on operator disconnect.
	void clear();

	/// Unblock all waiters and refuse further waits/pushes (module teardown).
	void shutdown();

private:
	std::mutex mutex_;
	std::condition_variable cv_;
	/// Single coalescing slot: push() always supersedes any unconsumed command, so at most one is held.
	std::optional<OperatorCommand> pending_;
	bool shutdown_{false};
};

} // namespace bringauto::transparent_module_utils::operator_stream
