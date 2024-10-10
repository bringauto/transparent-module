#include <fleet_protocol/common_headers/general_error_codes.h>
#include <fleet_protocol/common_headers/memory_management.h>

#include <cstdint>
#include <cstdlib>
#include <iostream>

int allocate(buffer *buffer_pointer, size_t size_in_bytes)
{
    try
    {
        buffer_pointer->data = new uint8_t[size_in_bytes];
    }
    catch (const std::bad_alloc &)
    {
        return NOT_OK;
    }
    buffer_pointer->size_in_bytes = size_in_bytes;
    return OK;
}

void deallocate(buffer *buffer_pointer)
{
    if (buffer_pointer == nullptr)
    {
        return;
    }
    if (buffer_pointer->data != nullptr)
    {
        delete[] static_cast<uint8_t *>(buffer_pointer->data);
    }
    buffer_pointer->data = nullptr;
    buffer_pointer->size_in_bytes = 0;
}
