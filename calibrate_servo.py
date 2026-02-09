from machine import Pin, PWM
import time

print("\n" + "="*60)
print("SERVO CALIBRATION TOOL")
print("="*60)

servo = PWM(Pin(13), freq=50)

print("\nThis will help you find the correct duty cycle values")
print("for your servo's 0°, 90°, and 180° positions.\n")

def test_duty(duty):
    """Test a specific duty cycle value"""
    servo.duty(duty)
    print(f"Testing duty: {duty}")
    time.sleep(2)

print("="*60)
print("STEP 1: Find MINIMUM (0° - Full Left)")
print("="*60)
print("\nStarting from duty = 20 and going up...")
print("Watch your servo. Stop when it reaches the LEFTMOST position.\n")

for duty in range(20, 50, 2):
    test_duty(duty)
    print(f"  → Is this 0° (full left)? Current duty: {duty}")
    time.sleep(1)

print("\n" + "="*60)
print("STEP 2: Find CENTER (90°)")
print("="*60)
print("\nTesting center range...\n")

for duty in range(60, 90, 2):
    test_duty(duty)
    print(f"  → Is this 90° (center)? Current duty: {duty}")
    time.sleep(1)

print("\n" + "="*60)
print("STEP 3: Find MAXIMUM (180° - Full Right)")
print("="*60)
print("\nTesting maximum range...\n")

for duty in range(100, 140, 2):
    test_duty(duty)
    print(f"  → Is this 180° (full right)? Current duty: {duty}")
    time.sleep(1)

print("\n" + "="*60)
print("CALIBRATION COMPLETE")
print("="*60)
print("\nWrite down the THREE duty values you observed:")
print("  - 0° (leftmost):  duty = ___")
print("  - 90° (center):   duty = ___")
print("  - 180° (rightmost): duty = ___")
print("\nThen update your main.py with these values!")
print("="*60)