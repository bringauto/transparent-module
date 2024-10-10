#pragma once

#include <bringauto/fleet_protocol/cxx/DeviceID.hpp>
#include <bringauto/fleet_protocol/http_client/FleetApiClient.hpp>

#include <condition_variable>
#include <string>
#include <thread>
#include <vector>

namespace bringauto::transparent_module_utils
{

    struct context
    {
        std::shared_ptr<bringauto::fleet_protocol::http_client::FleetApiClient> fleet_api_client;
        std::vector<device_identification> devices;
        std::vector<std::pair<std::string, bringauto::fleet_protocol::cxx::DeviceID>> command_vector;
        std::mutex mutex;
        std::condition_variable con_variable;
        long last_command_timestamp;
    };

} // namespace bringauto::transparent_module_utils
