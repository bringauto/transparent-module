# Transparent Module

The Transparent Module serves primarily for testing purposes. It has no specific internal function beyond relaying data from input to output, making it ideal for testing.

## Module specification

- Module ID: 3
- Module does not implement any logic for specific device types.
- All device numbers are valid but do not affect the module's function.
- Names and roles of connected devices must match the following regular expression: `^[a-z0-9_]*$`.
- As the default command (the command that is sent before any command is received), the module sends the raw string `"default_command"`.

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

Where `device_id.name`, `device_id.role` and `device_id.type` have to be the same as the connected device.
And in `payload.data` you can put any string you want to send.

## Sending status

The module represents all sent data as strings, including JSON objects. When sending a JSON, it is serialized (dumped) into a string format before being sent. This allows the module to handle all data types uniformly, ensuring flexibility.

## Example

An example script, `example.py`, is provided in the `example` subdirectory. This script demonstrates how the module can be utilized for sending data between devices.
