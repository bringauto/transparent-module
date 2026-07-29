#include <bringauto/transparent_module_utils/operator_stream/OperatorChannel.hpp>

namespace bringauto::transparent_module_utils::operator_stream
{

    void OperatorChannel::push(OperatorCommand command)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (shutdown_)
            {
                return;
            }
            // Only the freshest command matters — coalesce by dropping any not-yet-consumed command so
            // wait_for_command never forwards a stale command a newer one already superseded.
            pending_ = std::move(command);
        }
        cv_.notify_one();
    }

    std::optional<OperatorCommand> OperatorChannel::waitForCommand(std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        const bool ready = cv_.wait_for(lock, timeout, [this] { return shutdown_ || pending_.has_value(); });
        if (!ready || shutdown_ || !pending_)
        {
            return std::nullopt;
        }
        OperatorCommand command = std::move(*pending_);
        pending_.reset();
        return command;
    }

    void OperatorChannel::clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_.reset();
    }

    void OperatorChannel::shutdown()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            shutdown_ = true;
            pending_.reset();
        }
        cv_.notify_all();
    }

} // namespace bringauto::transparent_module_utils::operator_stream
