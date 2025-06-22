# PI Pico Trigger Sweeper
This tool is using a Raspberry Pi Pico to generate a trigger pulse of fixed length from an incoming trigger signal.
The PIO blocks of the pico are used to archieve consistent timing. The output trigger duration can be configured over serial with a **resolution of 8 ns** (1 clock cycle of the pi pico). The output trigger signal will always be **offset by 40 ns** from the input trigger signal.

## Wiring
- trigger input -> GPIO_0
- trigger output -> GPIO_1

## Build Instructions
```
export PICO_SDK_PATH=/opt/pico-sdk
mkdir build
cd build
cmake ..
make
```


## Usage
The output trigger duration can be controlled in clock cycles of the pi pico (8 ns each).
By default the output trigger duration is set to 10 clock cycles (= 80 ns).

The `delay_control.py` can be used to set or get the output trigger duration over serial.

**In bash commandline:**
```bash
python3 test.py --port /dev/ttyACM0 --get

python3 test.py --port /dev/ttyACM0 --set 20
```

**In python:**
```python
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
```
