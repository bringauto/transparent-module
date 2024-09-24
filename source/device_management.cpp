#include <bringauto/modules/transparent_module/transparent_module.hpp>
#include <fleet_protocol/common_headers/device_management.h>
#include <fleet_protocol/common_headers/general_error_codes.h>

int get_module_number() { return bringauto::modules::transparent_module::TRANSPARENT_MODULE_NUMBER; }

int is_device_type_supported(unsigned int device_type) { return OK; }
