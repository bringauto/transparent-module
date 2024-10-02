#include <bringauto/modules/transparent_module/devices/testing_device/testing_module_manager.hpp>
#include <fleet_protocol/module_maintainer/module_gateway/module_manager.h>

#include <cstring>
#include <iostream>

int send_status_condition(const buffer current_status, const buffer new_status, unsigned int device_type)
{
    return bringauto::modules::transparent_module::devices::testing_device::testing_device_send_status_condition(
        current_status, new_status);
}

int generate_command(buffer *generated_command, const buffer new_status, const buffer current_status,
                     const buffer current_command, unsigned int device_type)
{
    return bringauto::modules::transparent_module::devices::testing_device::testing_device_generate_command(
        generated_command, new_status, current_status, current_command);
}


int aggregate_status(buffer *aggregated_status, const buffer current_status, const buffer new_status,
                     unsigned int device_type)
{
    return bringauto::modules::transparent_module::devices::testing_device::testing_device_aggregate_status(
        aggregated_status, current_status, new_status);
}

int aggregate_error(buffer *error_message, const buffer current_error_message, const buffer status,
                    unsigned int device_type)
{
    return bringauto::modules::transparent_module::devices::testing_device::testing_device_aggregate_error(
        error_message, current_error_message, status);
}


int generate_first_command(buffer *default_command, unsigned int device_type)
{
    return bringauto::modules::transparent_module::devices::testing_device::testing_device_generate_first_command(
        default_command);
}

int status_data_valid(const buffer status, unsigned int device_type)
{
    return bringauto::modules::transparent_module::devices::testing_device::testing_device_status_data_valid(status);
}

int command_data_valid(const buffer command, unsigned int device_type)
{
    return bringauto::modules::transparent_module::devices::testing_device::testing_device_command_data_valid(command);
}
