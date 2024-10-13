#include <fleet_protocol/module_maintainer/module_gateway/module_manager.h>

#include <nlohmann/json.hpp>    

#include <cstring>
#include <iostream>
#include <string>

using json = nlohmann::ordered_json;

namespace bringauto::modules::transparent_module::devices::testing_device
{

    int testing_device_send_status_condition(const buffer current_status, const buffer new_status)
    {
        // This function does not impose any conditions on the status change.
        // It exists solely for compatibility purposes.
        return OK;
    }

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
        json j = std::string(static_cast<char *>(new_status.data), new_status.size_in_bytes);
        std::string str = to_string(j);

        if (allocate(aggregated_status, str.size()) == NOT_OK)
        {
            return NOT_OK;
        }
        std::memcpy(aggregated_status->data, str.c_str(), str.size());

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
        size_t command_length = strlen(command);

        if (allocate(default_command, command_length) == NOT_OK)
        {
            return NOT_OK;
        }
        std::memcpy(default_command->data, command, command_length);
        return OK;
    }

    int testing_device_status_data_valid(const buffer status)
    {
        // This function does not perform any validation on the status data.
        // It is provided for compatibility purposes.
        return OK;
    }

    int testing_device_command_data_valid(const buffer command)
    {
        // This function does not perform any validation on the command data.
        // It is provided for compatibility purposes.
        return OK;
    }

} // namespace bringauto::modules::transparent_module::devices::testing_device
