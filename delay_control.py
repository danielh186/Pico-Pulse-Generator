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


    valid_param_keys = {"offset", "length", "spacing", "repeats"}

    def set_parameters(self, parameters: dict):
        uart_string = "S "
        for key, value in parameters.items():
            if key in self.valid_param_keys:
                uart_string += f"{key[0]} {value} "

        self.ser.write(uart_string.encode('ascii'))
        response = self.ser.readline().decode().strip()

        if response != "OK":
            raise RuntimeError(f"Setting Pico parameters: '{response}' on command: '{uart_string}'")


    def get_parameter(self, key):
        if key in self.valid_param_keys:
            uart_string = f"G {key[0]}"
            self.ser.write(uart_string.encode('ascii'))
            response = self.ser.readline().decode().strip()
            try:
                return int(response)
            except ValueError:
                print(f"Error: Unexpected response '{response}'")
                return -1
        else:
            return -1


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