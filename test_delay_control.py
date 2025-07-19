from delay_control import DelayController
import serial

try:
    with DelayController() as dc:
        # Set parameters
        dc.set_parameters({"offset": 15, "length": 20, "spacing": 15, "repeats": 2})

        # Get parameters
        offset = dc.get_parameter("offset")
        length = dc.get_parameter("length")
        spacing = dc.get_parameter("spacing")
        repeats = dc.get_parameter("repeats")

        # Print parameters
        print(f"offset: {offset}")
        print(f"length: {length}")
        print(f"spacing: {spacing}")
        print(f"repeats: {repeats}")

except serial.SerialException as e:
    print(f"Serial error: {e}")
except ValueError as e:
    print(f"Communication error: {e}")