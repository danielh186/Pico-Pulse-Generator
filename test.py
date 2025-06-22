from delay_control import DelayController

delay = 5
try:
    with DelayController() as dc:
        if not dc.set_delay(delay):
            raise RuntimeError("Failed to set delay")

        actual = dc.get_delay()
        print(f"Delay set to {actual}")

except serial.SerialException as e:
    print(f"Serial error: {e}")
except ValueError as e:
    print(f"Communication error: {e}")