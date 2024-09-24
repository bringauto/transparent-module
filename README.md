# Transparent Module

The Transparent Module serves primarily for testing purposes. It has no specific internal function beyond relaying data from input to output, making it ideal for testing.

## Module specification

- Module ID: 3
- Module do not implement any logic for specific device types.
- All device numbers are valid but do not affect the module's function.
- Names and roles of connected devices must match the following regular expression: `^[a-z0-9_]*$`.

## Requirements

- [CMakeLib](https://github.com/cmakelib/cmakelib)

## Build

```bash
mkdir _build && cd _build
cmake .. -DCMLIB_DIR=<path-to-cmakelib-dir>
make
```

## Command structure

To send data from `http_api` to the module, use the following JSON structure:

```json
[
  {
    "device_id":     {
      "module_id": 3,
      "name": "yellow_testing_device",
      "role": "testing_device",
      "type": 0
    },
    "payload": {
      "data": "your payload",
      "encoding": "BASE64",
      "message_type": "COMMAND"
    }
  }
]
```

Where `device_id.name`, `device_id.role` and `device_id.type` have to be same as the connected device.
And in `payload.data` you can put any string you want to send.

## Example

An example script, `example.py`, is provided in the `example` subdirectory. This script demonstrates how the module can be utilized for sending data between devices.
