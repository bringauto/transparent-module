#include <bringauto/modules/transparent_module/devices/testing_device/testing_module_manager.hpp>
#include <fleet_protocol/module_maintainer/module_gateway/module_manager.h>

#include <cstring>
#include <iostream>

namespace td = bringauto::modules::transparent_module::devices::testing_device;

int send_status_condition(const buffer current_status, const buffer new_status, unsigned int device_type)
{
    return td::testing_device_send_status_condition(current_status, new_status);
}

int generate_command(buffer *generated_command, const buffer new_status, const buffer current_status,
                     const buffer current_command, unsigned int device_type)
{
    return td::testing_device_generate_command(generated_command, new_status, current_status, current_command);
}


int aggregate_status(buffer *aggregated_status, const buffer current_status, const buffer new_status,
                     unsigned int device_type)
{
    return td::testing_device_aggregate_status(aggregated_status, current_status, new_status);
}

int aggregate_error(buffer *error_message, const buffer current_error_message, const buffer status,
                    unsigned int device_type)
{
    return td::testing_device_aggregate_error(error_message, current_error_message, status);
}


int generate_first_command(buffer *default_command, unsigned int device_type)
{
    return td::testing_device_generate_first_command(default_command);
}

int status_data_valid(const buffer status, unsigned int device_type)
{
    return td::testing_device_status_data_valid(status);
}

int command_data_valid(const buffer command, unsigned int device_type)
{
    return td::testing_device_command_data_valid(command);
}

// BAF-1744: opt into MG's push-only forwarding (mirrors teleop-module/source/module_manager.cpp).
// Without this, MG's default fallback (fleet_protocol/module_maintainer/module_gateway/
// module_manager.h's forward_command_on_receive doc comment) re-invokes generate_command() with
// the same current_command on every single DeviceStatus when there's nothing new queued from the
// External Server — and testing_device_generate_command() is a pure passthrough that echoes
// current_command right back, so the device sees the same command redelivered on every status
// tick forever. Returning OK here switches MG to push-only mode: a new command from ES is
// forwarded immediately (ModuleHandler::handleCommandForward), and a status with nothing new gets
// an EMPTY DeviceCommand back (ModuleHandler::handleStatus) instead of the stale one.
int forward_command_on_receive(unsigned int device_type)
{
    return OK;
}
