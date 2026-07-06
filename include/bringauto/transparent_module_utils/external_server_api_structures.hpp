#pragma once

#include <bringauto/fleet_protocol/cxx/DeviceID.hpp>
#include <bringauto/fleet_protocol/http_client/FleetApiClient.hpp>
#include <bringauto/transparent_module_utils/operator_stream/OperatorChannel.hpp>
#include <bringauto/transparent_module_utils/operator_stream/QuicOperatorServer.hpp>

#include <condition_variable>
#include <memory>
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

        /// Operator-facing QUIC transport (BAF-1744) — used instead of the Fleet HTTP API above
        /// when the config supplies quic_port; otherwise both stay unused/idle. Declared in this
        /// order so operator_channel outlives quic_server (the server holds a reference to it).
        operator_stream::OperatorChannel operator_channel;
        std::unique_ptr<operator_stream::QuicOperatorServer> quic_server;
    };

} // namespace bringauto::transparent_module_utils
