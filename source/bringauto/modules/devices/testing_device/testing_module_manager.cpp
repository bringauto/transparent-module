#include <fleet_protocol/module_maintainer/module_gateway/module_manager.h>

#include <cstring>
#include <iostream>
#include <string>

// bringauto::modules::transparent_module::testing_module_manager
namespace bringauto::modules::transparent_module::devices::testing_device
{

    int testing_device_send_status_condition(const buffer current_status, const buffer new_status) { return OK; }

    int testing_device_generate_command(buffer *generated_command, const buffer new_status, const buffer current_status,
                                        const buffer current_command)
    {
        if (allocate(generated_command, current_command.size_in_bytes) == NOT_OK)
        {
            return NOT_OK;
        }
        std::memcpy(generated_command->data, current_command.data, generated_command->size_in_bytes);

        return OK;
    }

    int testing_device_aggregate_status(buffer *aggregated_status, const buffer current_status, const buffer new_status)
    {
        if (allocate(aggregated_status, new_status.size_in_bytes) == NOT_OK)
        {
            return NOT_OK;
        }
        std::memcpy(aggregated_status->data, new_status.data, aggregated_status->size_in_bytes);

        return OK;
    }

    int testing_device_aggregate_error(buffer *error_message, const buffer current_error_message, const buffer status)
    {
        if (allocate(error_message, 1) == NOT_OK)
        {
            return NOT_OK;
        }
        return OK;
    }

    int testing_device_generate_first_command(buffer *default_command)
    {
        const char *command = R"("default_command")";

        if (allocate(default_command, strlen(command)) == NOT_OK)
        {
            return NOT_OK;
        }

        std::memcpy(default_command->data, command, default_command->size_in_bytes);
        return OK;
    }

    int testing_device_status_data_valid(const buffer status) { return OK; }

    int testing_device_command_data_valid(const buffer command) { return OK; }

} // namespace bringauto::modules::transparent_module::devices::testing_device
