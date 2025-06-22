#!/usr/bin/env python3
import serial
import struct
import time
import argparse

class DelayController:
    def __init__(self, port='/dev/ttyACM0', baudrate=115200):
        self.ser = serial.Serial(port, baudrate, timeout=1)
        # time.sleep(2)  # Wait for Pico to initialize

    # for usage with content manager (`with PicoController() as pico: ....`)
    def __enter__(self):
        return self
    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def get_delay(self):
        self.ser.write(b'G')
        response = self.ser.readline().decode().strip()
        try:
            return int(response)
        except ValueError:
            print(f"Error: Unexpected response '{response}'")
            return None

    def set_delay(self, delay):
        if delay <= 0:
            print("Error: Delay must be positive")
            return False

        # Send command 'S' followed by 4-byte little-endian value
        self.ser.write(b'S' + struct.pack('<I', delay))
        response = self.ser.readline().decode().strip()

        if response == "OK":
            return True
        print(f"Error: {response}")
        return False

    def close(self):
        self.ser.close()

def main():
    parser = argparse.ArgumentParser(description='Control Pico delay parameter')
    parser.add_argument('--port', default='/dev/ttyUSB0', help='Serial port')
    parser.add_argument('--get', action='store_true', help='Get current delay')
    parser.add_argument('--set', type=int, help='Set new delay value')

    args = parser.parse_args()

    pico = DelayController(port=args.port)

    try:
        if args.get:
            delay = pico.get_delay()
            if delay is not None:
                print(f"Current delay: {delay}")
        elif args.set is not None:
            if pico.set_delay(args.set):
                print(f"Delay set to {args.set}")
        else:
            print("No action specified. Use --get or --set")
    finally:
        pico.close()

if __name__ == '__main__':
    main()