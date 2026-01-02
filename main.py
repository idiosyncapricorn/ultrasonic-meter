from machine import Pin
import time
import network
import socket

TRIG = Pin(27, Pin.OUT)
ECHO = Pin(26, Pin.IN, Pin.PULL_DOWN)
LED = Pin(2, Pin.OUT)
TRIG.value(0)

current_distance = 0
sensing_active = False  # Start with sensing OFF

ap = network.WLAN(network.AP_IF)
ap.active(True)
ap.config(essid='ESP32-Sensor', password='12345678')
print("WiFi: ESP32-Sensor | Password: 12345678 | Go to: 192.168.4.1")

def measure_distance():
    global current_distance
    TRIG.value(1)
    time.sleep_us(10)
    TRIG.value(0)

    timeout = time.ticks_us()
    while ECHO.value() == 0:
        if time.ticks_diff(time.ticks_us(), timeout) > 3000:
            return -1

    start_time = time.ticks_us()
    while ECHO.value() == 1:
        if time.ticks_diff(time.ticks_us(), start_time) > 30000:
            return -1

    distance = (time.ticks_diff(time.ticks_us(), start_time) * 0.0343) / 2
    LED.value(1 if distance < 10 else 0)
    return distance

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(('', 80))
s.listen(5)
s.setblocking(False)

last_time = time.ticks_ms()

while True:
    # Only measure distance if sensing is active
    if sensing_active and time.ticks_diff(time.ticks_ms(), last_time) > 100:
        d = measure_distance()
        if d > 0:
            current_distance = round(d, 1)
            print(f"Distance: {current_distance} cm")
        last_time = time.ticks_ms()

    try:
        conn, addr = s.accept()
        conn.settimeout(1.0)
        req = conn.recv(1024).decode()

        if 'GET /start' in req:
            sensing_active = True
            LED.value(0)
            print("Sensing started")
            conn.send('HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nOK')
        elif 'GET /stop' in req:
            sensing_active = False
            LED.value(0)
            current_distance = 0
            print("Sensing stopped")
            conn.send('HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nOK')
        elif 'GET /status' in req:
            conn.send('HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n')
            conn.sendall('{"active":' + ('true' if sensing_active else 'false') + '}')
        elif 'GET /distance' in req:
            conn.send('HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\n')
            conn.sendall(f'{current_distance}')
        else:
            conn.send('HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n')
            with open("index.html", "r") as f:
                html = f.read()
            conn.sendall(html)

        conn.close()
    except:
        pass

    time.sleep_ms(10)