#!/usr/bin/env python3
import serial
import struct
import time
import argparse

class DelayController:
    def __init__(self, port='/dev/ttyACM0', baudrate=115200):
        self.ser = serial.Serial(port, baudrate, timeout=1)

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def close(self):
        self.ser.close()


    # Since pico expects paramets in clock cylces and one clock cycle = 5ns
    # Divide incoming ns values by 5
    param_constraints = {
        "offset":  {"range": (2 * 5, (2**32 - 1) * 5), "divider": 5},
        "length":  {"range": (1 * 5, (2**7 - 1) * 5),  "divider": 5},
        "spacing": {"range": (6 * 6, (2**20 - 1) * 5),  "divider": 5},
        "repeats": {"range": (0, 31),   "divider": 1},
    }

    def set_parameters(self, parameters: dict):
        uart_string = "S "
        for key, value in parameters.items():
            # Verify parameter name
            if key not in self.param_constraints:
                raise ValueError(f"Invalid parameter: '{key}'")

            # Get parameter constraints
            constraint = self.param_constraints[key]
            value_range = constraint["range"]
            divider = constraint["divider"]

            # Verify parameter type (int)
            if not isinstance(value, int):
                raise ValueError(f"Value for '{key}' must be an integer.")

            # Verify parameter range
            if value_range:
                min_val, max_val = value_range
                if not (min_val <= value <= max_val):
                    raise ValueError(f"Value for '{key}'={value} is out of valid range {value_range}.")
            if divider and value % divider != 0:
                raise ValueError(f"Value for '{key}'={value} must be divisible by {divider}.")

            divided_value = value // divider # Calculate clock cycles from ns inputs
            uart_string += f"{key[0]} {divided_value} "

        self.ser.write(uart_string.encode('ascii'))
        response = self.ser.readline().decode().strip()

        if response != "OK":
            raise RuntimeError(f"Setting Pico parameters: '{response}' on command: '{uart_string}'")

    def get_parameter(self, key):
        # Veriy parameter name
        if key not in self.param_constraints:
            raise ValueError(f"Invalid parameter: '{key}'")

        # Get parameter constraints
        divider = self.param_constraints[key]["divider"]
        uart_string = f"G {key[0]}"
        self.ser.write(uart_string.encode('ascii'))
        response = self.ser.readline().decode().strip()

        # Convert parameter value to number
        try:
            raw = int(response)
        except ValueError:
            print(f"Error: Unexpected response '{response}' for parameter '{key}'")
            return -1

        # Return paramter in ns (multiply by 5)
        return raw * divider

def main():
    parser = argparse.ArgumentParser(description='Control delay parameters on a Raspberry Pi Pico')
    parser.add_argument('--port', default='/dev/ttyACM0', help='Serial port')
    parser.add_argument('--get', nargs='*', choices=['offset', 'length', 'spacing', 'repeats'],
                        help='Get one or more parameters')
    parser.add_argument('--set', nargs='*', metavar='PARAM=VALUE',
                        help='Set one or more parameters (e.g. --set offset=100 length=200)')

    args = parser.parse_args()

    with DelayController(port=args.port) as dc:
            # Set parameters
            if args.set:
                params = {}
                for item in args.set:
                    try:
                        key, val = item.split('=')
                        val = int(val)
                        params[key] = val
                    except ValueError:
                        print(f"Invalid format or value: '{item}' (expected PARAM=VALUE)")
                if params:
                    try:
                        dc.set_parameters(params)
                        for k, v in params.items():
                            print(f"Set {k} = {v}")
                    except RuntimeError as e:
                        print(f"Error: {e}")

            # Get parameters
            if args.get:
                for key in args.get:
                    val = dc.get_parameter(key)
                    if val is not None:
                        print(f"{key} = {val}")

            if not args.get and not args.set:
                print("No action specified. Use --get or --set.")


if __name__ == '__main__':
    main()