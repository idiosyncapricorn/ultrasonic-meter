from machine import Pin, PWM
import time
import network
import socket
import gc

# ============================================================================
# HARDWARE PINS
# ============================================================================

# HC-SR04 Ultrasonic Sensor
TRIG = Pin(27, Pin.OUT)
ECHO = Pin(26, Pin.IN, Pin.PULL_DOWN)
LED = Pin(2, Pin.OUT)
TRIG.value(0)

# SERVO
SERVO = PWM(Pin(13), freq=50)

# ============================================================================
# GLOBAL VARIABLES
# ============================================================================

current_distance = 0
current_angle = 90
sweep_enabled = True  # Servo sweeps automatically on startup
sensing_active = True  # Distance sensing active on startup

# Servo sweep state
sweep_direction = 1  # 1 = increasing, -1 = decreasing
sweep_angle = 0
sweep_speed = 5  # Degrees to move per step (lower = smoother, slower)

# ============================================================================
# CALIBRATION VALUES - ADJUST FOR YOUR SERVO
# ============================================================================
SERVO_MIN_DUTY = 26   # Your 0° value
SERVO_MAX_DUTY = 128  # Your 180° value

# ============================================================================
# WIFI ACCESS POINT
# ============================================================================

ap = network.WLAN(network.AP_IF)
ap.active(True)
ap.config(essid='ESP32Dick', password='12345678', channel=11)
print("WiFi: ESP32Dick | Password: 12345678 | Go to: 192.168.4.1")

# ============================================================================
# LOAD HTML
# ============================================================================

try:
    with open('index.html', 'r') as f:
        webpage = f.read()
except:
    webpage = """<!DOCTYPE html>
<html><body>
<h1>Error: index.html not found</h1>
<p>Please upload index.html to ESP32</p>
</body></html>"""
    print("Warning: index.html not found on ESP32")

# ============================================================================
# SERVO FUNCTIONS
# ============================================================================

def set_servo_angle(angle):
    """Set servo to specific angle (0-180°)"""
    global current_angle

    if angle < 0:
        angle = 0
    elif angle > 180:
        angle = 180

    # Calculate duty cycle from angle using calibrated values
    duty = int(SERVO_MIN_DUTY + (angle / 180.0) * (SERVO_MAX_DUTY - SERVO_MIN_DUTY))
    SERVO.duty(duty)

    current_angle = angle

def update_sweep():
    """Update servo position for continuous sweeping"""
    global sweep_angle, sweep_direction

    if not sweep_enabled:
        return

    # Move servo
    sweep_angle += sweep_speed * sweep_direction

    # Reverse direction at limits
    if sweep_angle >= 180:
        sweep_angle = 180
        sweep_direction = -1
    elif sweep_angle <= 0:
        sweep_angle = 0
        sweep_direction = 1

    set_servo_angle(sweep_angle)

# ============================================================================
# DISTANCE MEASUREMENT
# ============================================================================

def measure_distance():
    """Measure distance with HC-SR04"""
    global current_distance

    TRIG.value(1)
    time.sleep_us(10)
    TRIG.value(0)

    timeout = time.ticks_us()
    while ECHO.value() == 0:
        if time.ticks_diff(time.ticks_us(), timeout) > 30000:
            return -1

    start_time = time.ticks_us()
    while ECHO.value() == 1:
        if time.ticks_diff(time.ticks_us(), start_time) > 30000:
            return -1

    distance = (time.ticks_diff(time.ticks_us(), start_time) * 0.0343) / 2
    LED.value(1 if distance < 10 else 0)
    return distance

# ============================================================================
# WEB SERVER
# ============================================================================

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(('', 80))
s.listen(10)
s.setblocking(False)

# ============================================================================
# STARTUP
# ============================================================================

print("\n" + "="*60)
print("ESP32 CONTINUOUS SWEEP + DISTANCE SENSOR")
print("="*60)
print("Features:")
print("  • Servo sweeps continuously (0° to 180° and back)")
print("  • Ultrasonic sensor measures distance constantly")
print("  • Web interface for control and monitoring")
print("="*60)

print("\nInitializing servo...")
set_servo_angle(0)
time.sleep(1)

print("✓ System ready!")
print("✓ Servo sweeping automatically")
print("✓ Distance sensing active")
print("Connect to WiFi and go to: http://192.168.4.1")
print("="*60 + "\n")

# ============================================================================
# MAIN LOOP - PARALLEL OPERATION
# ============================================================================

last_distance_time = time.ticks_ms()
last_sweep_time = time.ticks_ms()
last_print_time = time.ticks_ms()

DISTANCE_INTERVAL = 100  # Measure distance every 100ms
SWEEP_INTERVAL = 50      # Update servo every 50ms (smooth motion)
PRINT_INTERVAL = 1000    # Print status every 1 second

print("Starting main loop...")
print("Format: Distance: XX.X cm | Servo: XXX°\n")

while True:
    current_time = time.ticks_ms()

    # ========================================================================
    # TASK 1: DISTANCE SENSING (every 100ms)
    # ========================================================================
    if sensing_active and time.ticks_diff(current_time, last_distance_time) >= DISTANCE_INTERVAL:
        d = measure_distance()
        if d > 0:
            current_distance = round(d, 1)
        last_distance_time = current_time

    # ========================================================================
    # TASK 2: SERVO SWEEP (every 50ms)
    # ========================================================================
    if sweep_enabled and time.ticks_diff(current_time, last_sweep_time) >= SWEEP_INTERVAL:
        update_sweep()
        last_sweep_time = current_time

    # ========================================================================
    # TASK 3: STATUS PRINT (every 1 second)
    # ========================================================================
    if time.ticks_diff(current_time, last_print_time) >= PRINT_INTERVAL:
        print(f"Distance: {current_distance:5.1f} cm | Servo: {current_angle:3d}° | Sweep: {'ON' if sweep_enabled else 'OFF'} | Sense: {'ON' if sensing_active else 'OFF'}")
        last_print_time = current_time

    # ========================================================================
    # TASK 4: WEB SERVER (non-blocking)
    # ========================================================================
    try:
        conn, addr = s.accept()
        conn.settimeout(1.0)

        try:
            req = conn.recv(1024).decode()

            # Toggle sweep on/off
            if 'GET /sweep/start' in req:
                sweep_enabled = True
                print("✓ Sweep ENABLED")
                conn.send('HTTP/1.1 200 OK\r\n\r\nSweep started')

            elif 'GET /sweep/stop' in req:
                sweep_enabled = False
                print("✗ Sweep DISABLED")
                conn.send('HTTP/1.1 200 OK\r\n\r\nSweep stopped')

            # Toggle sensing on/off
            elif 'GET /sense/start' in req:
                sensing_active = True
                print("✓ Sensing ENABLED")
                conn.send('HTTP/1.1 200 OK\r\n\r\nSensing started')

            elif 'GET /sense/stop' in req:
                sensing_active = False
                current_distance = 0
                print("✗ Sensing DISABLED")
                conn.send('HTTP/1.1 200 OK\r\n\r\nSensing stopped')

            # Manual servo control (stops sweep temporarily)
            elif 'GET /servo/angle/' in req:
                try:
                    angle = int(req.split('/servo/angle/')[1].split(' ')[0])
                    sweep_enabled = False  # Stop auto-sweep
                    set_servo_angle(angle)
                    print(f"Manual servo: {angle}°")
                    conn.send(f'HTTP/1.1 200 OK\r\n\r\nServo: {angle}°')
                except:
                    conn.send('HTTP/1.1 400 Bad Request\r\n\r\nInvalid')

            # Get current distance
            elif 'GET /distance' in req:
                conn.send(f'HTTP/1.1 200 OK\r\n\r\n{current_distance}')

            # Get current status
            elif 'GET /status' in req:
                status = f'{{"angle":{current_angle},"distance":{current_distance},"sweeping":{str(sweep_enabled).lower()},"sensing":{str(sensing_active).lower()}}}'
                conn.send('HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n')
                conn.sendall(status)

            # Main webpage
            else:
                conn.send('HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n')
                conn.sendall(webpage)

        except Exception as e:
            print(f"Request error: {e}")
        finally:
            try:
                conn.close()
            except:
                pass

    except OSError:
        pass

    gc.collect()
    time.sleep_ms(10)  # Small delay to prevent CPU overload