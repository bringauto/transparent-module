import logging
from time import sleep

from internal_client import exceptions, InternalClient


def main():
    # create client instance and connect to server
    try:
        client = InternalClient(
            module_id=3,
            hostname="127.0.0.1",
            port="1636",
            device_name="yellow_testing_device",
            device_type=0,
            device_role="testing_device",
            device_priority=0,
        )
    except exceptions.CommunicationExceptions as e:
        logging.error(f"Couldn't connect to server: {e}.")
        return False
    except exceptions.ConnectExceptions as e:
        logging.error(f"Device could not be connected because server responded with: {type(e)}.")
        return False

    n = 0
    while True:
        my_status = f"Status {n}"

        try:
            client.send_status(my_status.encode(), timeout=10)
            n += 1

        except exceptions.ServerTookTooLong:
            logging.error("Server timed out, context invalid.")
            break
        except (exceptions.CommunicationExceptions, exceptions.ConnectExceptions):
            logging.error("Server error, context invalid.")
            break

        command = client.get_command()
        logging.info(f"Received command: {command}")
        sleep(5)
    client.destroy()


if __name__ == "__main__":
    main()
