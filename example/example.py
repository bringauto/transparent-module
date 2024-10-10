import logging
from time import sleep

from internal_client import exceptions, InternalClient

# Configure logging to output to the terminal
logging.basicConfig(
    level=logging.INFO,  # Set the minimum log level (INFO, ERROR, etc.)
    format='%(asctime)s - %(levelname)s - %(message)s'  # Log message format
)

def main():
    # create client instance and connect to server
    try:
        client = InternalClient(
            module_id=3,
            hostname="127.0.0.1",
            port=1636,
            device_name="yellow_testing_device",
            device_type=0,
            device_role="testing_device",
            device_priority=0,
        )
    except exceptions.CommunicationExceptions as e:
        logging.error(f"Couldn't connect to server: {e}.")
        return
    except exceptions.ConnectExceptions as e:
        logging.error(f"Device could not be connected because server responded with: {e}.")
        return

    n = 0
    try:
        while True:
            my_status = f"Status {n}"

            try:
                client.send_status(my_status.encode(), timeout=10)
                n += 1

            except exceptions.ServerTookTooLong as e:
                logging.error(f"Server timed out, context invalid: {e}")
                break
            except (exceptions.CommunicationExceptions, exceptions.ConnectExceptions) as e:
                logging.error(f"Server error, context invalid: {e}")
                break

            try:
                command = client.get_command()
                logging.info(f"Received command: {command}")
            except (exceptions.CommunicationExceptions, exceptions.ConnectExceptions) as e:
                logging.error(f"Failed to get command: {e}")
                break
            sleep(5)

    finally:
        client.destroy()
        logging.info("Client destroyed.")

if __name__ == "__main__":
    main()
