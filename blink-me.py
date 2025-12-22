from machine import Pin
import time

TRIG = Pin(27, Pin.OUT)
ECHO = Pin(26, Pin.IN, Pin.PULL_DOWN)
LED = Pin(2, Pin.OUT)

TRIG.value(0)

while True:
    # Trigger
    TRIG.value(1)
    time.sleep_us(10)
    TRIG.value(0)

    # Wait for echo start
    timeout = time.ticks_us()
    while ECHO.value() == 0:
        if time.ticks_diff(time.ticks_us(), timeout) > 3000:
            break

    # Measure pulse
    start = time.ticks_us()
    while ECHO.value() == 1:
        if time.ticks_diff(time.ticks_us(), start) > 30000:
            break

    pulse = time.ticks_diff(time.ticks_us(), start)

    print(f"Echo us: {pulse}")

    # Control LED
    LED.value(1 if pulse < 500 else 0)

    time.sleep(1)


