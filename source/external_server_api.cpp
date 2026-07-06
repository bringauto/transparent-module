#include <bringauto/fleet_protocol/cxx/KeyValueConfig.hpp>
#include <bringauto/fleet_protocol/cxx/StringAsBuffer.hpp>
#include <bringauto/fleet_protocol/http_client/FleetApiClient.hpp>
#include <bringauto/modules/transparent_module/transparent_module.hpp>
#include <bringauto/transparent_module_utils/external_server_api_structures.hpp>
#include <fleet_protocol/module_maintainer/external_server/external_server_interface.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <optional>
#include <regex>
#include <string>
#include <vector>

using namespace std::chrono_literals;

namespace
{
    namespace op = bringauto::transparent_module_utils::operator_stream;

    /// Parse the quic_* config keys into a QuicOperatorServerConfig. Returns std::nullopt if
    /// quic_port is absent/zero (BAF-1744: QUIC is opt-in alongside the existing Fleet HTTP API —
    /// see the header comment on context::quic_server).
    std::optional<op::QuicOperatorServerConfig> parseQuicConfig(
        const bringauto::fleet_protocol::cxx::KeyValueConfig &config)
    {
        op::QuicOperatorServerConfig quicConfig{};
        int port = 0;

        for (auto i = config.cbegin(); i != config.cend(); ++i)
        {
            if (i->first == "quic_port")
            {
                try
                {
                    port = std::stoi(i->second);
                }
                catch (const std::exception &)
                {
                    std::cerr << "[transparent-module] init: invalid quic_port" << std::endl;
                    return std::nullopt;
                }
            }
            else if (i->first == "quic_cert_path")
            {
                quicConfig.certPath = i->second;
            }
            else if (i->first == "quic_key_path")
            {
                quicConfig.keyPath = i->second;
            }
            else if (i->first == "quic_ca_path")
            {
                quicConfig.caCertsPath = i->second;
            }
            else if (i->first == "quic_alpn")
            {
                quicConfig.alpn = i->second;
            }
            else if (i->first == "quic_listen_address")
            {
                quicConfig.listenAddress = i->second;
            }
            else if (i->first == "quic_disable_client_auth")
            {
                quicConfig.disableClientAuth = (i->second == "true" || i->second == "1");
            }
        }

        if (port <= 0)
        {
            return std::nullopt; // QUIC not configured — caller falls back to Fleet HTTP API
        }
        if (port > 65535)
        {
            std::cerr << "[transparent-module] init: quic_port must be in range 1-65535" << std::endl;
            return std::nullopt;
        }
        quicConfig.port = static_cast<std::uint16_t>(port);

        if (quicConfig.certPath.empty() || quicConfig.keyPath.empty())
        {
            std::cerr << "[transparent-module] init: quic_port set but missing quic_cert_path/quic_key_path"
                      << std::endl;
            return std::nullopt;
        }
        if (!quicConfig.disableClientAuth && quicConfig.caCertsPath.empty())
        {
            std::cerr << "[transparent-module] init: quic_ca_path is required when client authentication is "
                         "enabled (set quic_ca_path, or quic_disable_client_auth=true for dev/loopback)"
                      << std::endl;
            return std::nullopt;
        }
        return quicConfig;
    }
} // namespace

void *init(const config config_data)
{
    auto *context = new struct bringauto::transparent_module_utils::context;
    bringauto::fleet_protocol::cxx::KeyValueConfig config(config_data);
    std::string api_url;
    std::string api_key;
    std::string company_name;
    std::string car_name;
    // Default values are set in case the configuration file does not contain the values and are based on values used in
    // external server configuration file
    int max_requests_threshold_count = 5;
    int max_requests_threshold_period_ms = 1000;
    int delay_after_threshold_reached_ms = 500;
    int retry_requests_delay_ms = 220;

    for (auto i = config.cbegin(); i != config.cend(); i++)
    {
        if (i->first == "api_url")
        {
            if (!std::regex_match(i->second, std::regex(R"(^(http|https)://([\w-]+\.)?+[\w-]+(:[0-9]+)?(/[\w-]*)?+$)")))
            {
                delete context;
                return nullptr;
            }
            api_url = i->second;
        }
        else if (i->first == "api_key")
        {
            if (i->second.empty())
            {
                delete context;
                return nullptr;
            }
            api_key = i->second;
        }
        else if (i->first == "company_name")
        {
            if (!std::regex_match(i->second, std::regex("^[a-z0-9_]*$")) || i->second.empty())
            {
                delete context;
                return nullptr;
            }
            company_name = i->second;
        }
        else if (i->first == "car_name")
        {
            if (!std::regex_match(i->second, std::regex("^[a-z0-9_]*$")) || i->second.empty())
            {
                delete context;
                return nullptr;
            }
            car_name = i->second;
        }
        else if (i->first == "max_requests_threshold_count")
        {
            try
            {
                max_requests_threshold_count = std::stoi(i->second);
                if (max_requests_threshold_count < 0 || i->second.empty())
                {
                    throw std::exception();
                }
            }
            catch (std::exception &e)
            {
                delete context;
                return nullptr;
            }
        }
        else if (i->first == "max_requests_threshold_period_ms")
        {
            try
            {
                max_requests_threshold_period_ms = std::stoi(i->second);
                if (max_requests_threshold_period_ms < 0 || i->second.empty())
                {
                    throw std::exception();
                }
            }
            catch (std::exception &e)
            {
                delete context;
                return nullptr;
            }
        }
        else if (i->first == "delay_after_threshold_reached_ms")
        {
            try
            {
                delay_after_threshold_reached_ms = std::stoi(i->second);
                if (delay_after_threshold_reached_ms < 0 || i->second.empty())
                {
                    throw std::exception();
                }
            }
            catch (std::exception &e)
            {
                delete context;
                return nullptr;
            }
        }
        else if (i->first == "retry_requests_delay_ms")
        {
            try
            {
                retry_requests_delay_ms = std::stoi(i->second);
                if (retry_requests_delay_ms < 0 || i->second.empty())
                {
                    throw std::exception();
                }
            }
            catch (std::exception &e)
            {
                delete context;
                return nullptr;
            }
        }
    }

    bringauto::fleet_protocol::http_client::FleetApiClient::FleetApiClientConfig fleet_api_config{
        .apiUrl = api_url, .apiKey = api_key, .companyName = company_name, .carName = car_name};

    bringauto::fleet_protocol::http_client::RequestFrequencyGuard::RequestFrequencyGuardConfig
        request_frequency_guard_config{
            .maxRequestsThresholdCount = max_requests_threshold_count,
            .maxRequestsThresholdPeriodMs = std::chrono::milliseconds(max_requests_threshold_period_ms),
            .delayAfterThresholdReachedMs = std::chrono::milliseconds(delay_after_threshold_reached_ms),
            .retryRequestsDelayMs = std::chrono::milliseconds(retry_requests_delay_ms)};

    context->fleet_api_client = std::make_shared<bringauto::fleet_protocol::http_client::FleetApiClient>(
        fleet_api_config, request_frequency_guard_config);

    context->last_command_timestamp = 0;

    // BAF-1744: QUIC operator transport is opt-in (present only if the config supplies quic_port) —
    // when absent, forward_status()/wait_for_command() fall back to the Fleet HTTP API above unchanged.
    if (auto quicConfig = parseQuicConfig(config))
    {
        context->quic_server =
            std::make_unique<op::QuicOperatorServer>(std::move(*quicConfig), context->operator_channel);
        if (!context->quic_server->initialize() || !context->quic_server->start())
        {
            std::cerr << "[transparent-module] init: QUIC operator server failed to initialize/start" << std::endl;
            delete context;
            return nullptr;
        }
        std::cerr << "[transparent-module] init: QUIC operator server enabled" << std::endl;
    }

    return context;
}

int destroy(void **context)
{
    if (*context == nullptr)
    {
        return NOT_OK;
    }
    auto con = reinterpret_cast<struct bringauto::transparent_module_utils::context **>(context);

    delete *con;
    *con = nullptr;
    return OK;
}

int forward_status(const buffer device_status, const device_identification device, void *context)
{
    if (context == nullptr)
    {
        return CONTEXT_INCORRECT;
    }

    auto con = static_cast<struct bringauto::transparent_module_utils::context *>(context);

    bringauto::fleet_protocol::cxx::BufferAsString device_role(&device.device_role);
    bringauto::fleet_protocol::cxx::BufferAsString device_name(&device.device_name);
    bringauto::fleet_protocol::cxx::BufferAsString device_status_str(&device_status);

    // BAF-1744: QUIC operator transport, when configured, replaces the Fleet HTTP API send below
    // for this call — see context::quic_server's doc comment.
    if (con->quic_server)
    {
        using SendResult = op::QuicOperatorServer::SendResult;
        const auto sendResult = con->quic_server->sendStatus(
            device.module, device.device_type, std::string(device_role.getStringView()),
            std::string(device_name.getStringView()), static_cast<const std::uint8_t *>(device_status.data),
            device_status.size_in_bytes, 0);
        // NoOperator is an expected best-effort drop (nobody connected) — report OK so the ES does
        // not treat a missing operator as a failure. A genuine send error is surfaced as NOT_OK.
        return sendResult == SendResult::SendFailed ? NOT_OK : OK;
    }

    con->fleet_api_client->setDeviceIdentification(bringauto::fleet_protocol::cxx::DeviceID(
        device.module, device.device_type,
        0, // priority
        std::string(device_role.getStringView()), std::string(device_name.getStringView())));

    try
    {
        auto str = std::string(device_status_str.getStringView());

        con->fleet_api_client->sendStatus(str);
    }
    catch (std::exception &e)
    {
        return NOT_OK;
    }

    con->con_variable.notify_one();

    return OK;
}

int forward_error_message(const buffer error_msg, const device_identification device, void *context)
{
    if (context == nullptr)
    {
        return CONTEXT_INCORRECT;
    }

    auto con = static_cast<struct bringauto::transparent_module_utils::context *>(context);

    bringauto::fleet_protocol::cxx::BufferAsString device_role(&device.device_role);
    bringauto::fleet_protocol::cxx::BufferAsString device_name(&device.device_name);
    bringauto::fleet_protocol::cxx::BufferAsString device_error_str(&error_msg);

    con->fleet_api_client->setDeviceIdentification(bringauto::fleet_protocol::cxx::DeviceID(
        device.module, device.device_type,
        0, // priority
        std::string(device_role.getStringView()), std::string(device_name.getStringView())));

    try
    {
        auto str = std::string(device_error_str.getStringView());
        con->fleet_api_client->sendStatus(
            str, bringauto::fleet_protocol::http_client::FleetApiClient::StatusType::STATUS_ERROR);
    }
    catch (std::exception &e)
    {
        return NOT_OK;
    }

    return OK;
}

int device_disconnected(const int disconnect_type, const device_identification device, void *context)
{
    if (context == nullptr)
    {
        return CONTEXT_INCORRECT;
    }

    auto con = static_cast<struct bringauto::transparent_module_utils::context *>(context);

    const std::string_view device_device_role(static_cast<char *>(device.device_role.data),
                                              device.device_role.size_in_bytes);
    const std::string_view device_device_name(static_cast<char *>(device.device_name.data),
                                              device.device_name.size_in_bytes);

    // BAF-1744: con->mutex now also guards con->devices for wait_for_command()'s QUIC branch,
    // which reads it from a different thread — this lock did not need to exist before that read
    // was added.
    std::lock_guard lock(con->mutex);
    for (auto it = con->devices.begin(); it != con->devices.end(); it++)
    {
        const std::string_view it_device_role(static_cast<char *>(it->device_role.data), it->device_role.size_in_bytes);
        const std::string_view it_device_name(static_cast<char *>(it->device_name.data), it->device_name.size_in_bytes);

        bool device_is_present = it->device_type == device.device_type && it_device_role == device_device_role &&
            it_device_name == device_device_name && it->module == device.module && it->priority == device.priority;

        if (device_is_present)
        {
            deallocate(&it->device_role);
            deallocate(&it->device_name);
            con->devices.erase(it);
            return OK;
        }
    }

    return NOT_OK;
}

int device_connected(const device_identification device, void *context)
{
    if (context == nullptr)
    {
        return CONTEXT_INCORRECT;
    }

    auto con = static_cast<struct bringauto::transparent_module_utils::context *>(context);

    device_identification new_device;

    new_device.module = device.module;
    new_device.device_type = device.device_type;
    new_device.priority = device.priority;

    if (allocate(&new_device.device_role, device.device_role.size_in_bytes) != OK)
    {
        return NOT_OK;
    }
    std::memcpy(new_device.device_role.data, device.device_role.data, new_device.device_role.size_in_bytes);

    if (allocate(&new_device.device_name, device.device_name.size_in_bytes) != OK)
    {
        deallocate(&new_device.device_role);
        return NOT_OK;
    }
    std::memcpy(new_device.device_name.data, device.device_name.data, new_device.device_name.size_in_bytes);

    // BAF-1744: see the matching lock in device_disconnected() for why this is needed now.
    std::lock_guard lock(con->mutex);
    con->devices.emplace_back(new_device);
    return OK;
}

int wait_for_command(int timeout_time_in_ms, void *context)
{
    if (context == nullptr)
    {
        return CONTEXT_INCORRECT;
    }

    auto con = static_cast<struct bringauto::transparent_module_utils::context *>(context);

    // BAF-1744: QUIC operator transport, when configured, replaces the Fleet HTTP polling below —
    // see context::quic_server's doc comment.
    if (con->quic_server)
    {
        constexpr int kDefaultWaitTimeoutMs = 1000;
        const int effectiveTimeoutMs = timeout_time_in_ms > 0 ? timeout_time_in_ms : kDefaultWaitTimeoutMs;
        auto command = con->operator_channel.waitForCommand(std::chrono::milliseconds(effectiveTimeoutMs));
        if (!command.has_value())
        {
            return TIMEOUT_OCCURRED;
        }

        std::lock_guard lock(con->mutex);
        // Transparent module doesn't discriminate by device_type (see its README: "All device
        // numbers are valid but do not affect the module's function") — route to the first
        // connected device on the addressed module.
        auto target = std::find_if(con->devices.begin(), con->devices.end(), [&](const device_identification &dev) {
            return static_cast<std::uint32_t>(dev.module) == command->module_id;
        });
        if (target == con->devices.end())
        {
            std::cerr << "[transparent-module] wait_for_command: operator command for module="
                      << command->module_id << " but no matching device connected, dropping" << std::endl;
            return TIMEOUT_OCCURRED;
        }

        bringauto::fleet_protocol::cxx::BufferAsString targetRole(&target->device_role);
        bringauto::fleet_protocol::cxx::BufferAsString targetName(&target->device_name);
        std::string payload(command->payload.begin(), command->payload.end());
        con->command_vector.emplace_back(
            std::move(payload), bringauto::fleet_protocol::cxx::DeviceID(
                                     target->module, target->device_type, target->priority,
                                     std::string(targetRole.getStringView()), std::string(targetName.getStringView())));
        return OK;
    }

    std::unique_lock lock(con->mutex);
    std::pair<std::vector<std::shared_ptr<org::openapitools::client::model::Message>>,
              bringauto::fleet_protocol::http_client::FleetApiClient::ReturnCode>
        commands;
    bool parse_commands = con->last_command_timestamp != 0;

    try
    {
        commands = con->fleet_api_client->getCommands(con->last_command_timestamp + 1, true);
    }
    catch (std::exception &e)
    {
        return TIMEOUT_OCCURRED;
    }

    bool received_no_commands = true;
    for (const auto &command : commands.first)
    {
        if (command->getTimestamp() > con->last_command_timestamp)
        {
            con->last_command_timestamp = command->getTimestamp();
        }

        auto received_device_id = command->getDeviceId();
        if (received_device_id->getModuleId() == bringauto::modules::transparent_module::TRANSPARENT_MODULE_NUMBER)
        {
            received_no_commands = false;
        }
        else
        {
            continue;
        }

        if (parse_commands)
        {
            std::string command_str = command->getPayload()->getData()->getJson().serialize();

            con->command_vector.emplace_back(command_str,
                                             bringauto::fleet_protocol::cxx::DeviceID(
                                                 received_device_id->getModuleId(), received_device_id->getType(),
                                                 0, // priority not returned from HTTP Api
                                                 received_device_id->getRole(), received_device_id->getName()));
        }
    }

    if (received_no_commands && !parse_commands)
    {
        con->last_command_timestamp = 1;
    }

    if (received_no_commands || !parse_commands)
    {
        return TIMEOUT_OCCURRED;
    }
    return OK;
}

int pop_command(buffer *command, device_identification *device, void *context)
{
    if (context == nullptr)
    {
        return CONTEXT_INCORRECT;
    }

    auto con = static_cast<struct bringauto::transparent_module_utils::context *>(context);
    if (con->command_vector.empty())
    {
        return NOT_OK;
    }
    auto command_object = std::get<0>(con->command_vector.back());

    auto &device_id = std::get<1>(con->command_vector.back());

    device->module = device_id.getDeviceId().module;
    device->device_type = device_id.getDeviceId().device_type;
    device->priority = device_id.getDeviceId().priority;

    if (allocate(&device->device_role, device_id.getDeviceId().device_role.size_in_bytes) == NOT_OK)
    {
        return NOT_OK;
    }
    std::memcpy(device->device_role.data, device_id.getDeviceId().device_role.data, device->device_role.size_in_bytes);

    if (allocate(&device->device_name, device_id.getDeviceId().device_name.size_in_bytes) == NOT_OK)
    {
        deallocate(&device->device_role);
        return NOT_OK;
    }
    std::memcpy(device->device_name.data, device_id.getDeviceId().device_name.data, device->device_name.size_in_bytes);

    auto command_string = con->command_vector.back().first;
    con->command_vector.pop_back();

    if (allocate(command, command_string.length()) == NOT_OK)
    {
        deallocate(&device->device_role);
        deallocate(&device->device_name);
        return NOT_OK;
    }

    bringauto::fleet_protocol::cxx::StringAsBuffer::createBufferAndCopyData(command, command_string);

    return static_cast<int>(con->command_vector.size());
}

int command_ack(const buffer command, const device_identification device, void *context)
{
    // This function is not used and is only there to work with the interface.
    return OK;
}
