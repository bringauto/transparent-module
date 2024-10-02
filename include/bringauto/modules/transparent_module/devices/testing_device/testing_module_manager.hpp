#pragma once

#include <fleet_protocol/common_headers/memory_management.h>

namespace bringauto::modules::transparent_module::devices::testing_device
{

    // These functions are implementing function defined in module_manager.h specifically for Arduino Uno device

    int testing_device_send_status_condition(const buffer current_status, const buffer new_status);

    int testing_device_generate_command(buffer *generated_command, const buffer new_status, const buffer current_status,
                                        const buffer current_command);

    int testing_device_aggregate_status(buffer *aggregated_status, const buffer current_status,
                                        const buffer new_status);

    int testing_device_aggregate_error(buffer *error_message, const buffer current_error_message, const buffer status);

    int testing_device_generate_first_command(buffer *default_command);

    int testing_device_status_data_valid(const buffer status);

    int testing_device_command_data_valid(const buffer command);

} // namespace bringauto::modules::transparent_module::devices::testing_device
