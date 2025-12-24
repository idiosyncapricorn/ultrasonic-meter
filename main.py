from machine import Pin
import time
import network
import socket

TRIG = Pin(27, Pin.OUT)
ECHO = Pin(26, Pin.IN, Pin.PULL_DOWN)
LED = Pin(2, Pin.OUT)
TRIG.value(0)

current_distance = 0

ap = network.WLAN(network.AP_IF)
ap.active(True)
ap.config(essid='ESP32-Sensor', password='12345678')

print("WiFi: ESP32-Sensor | Password: 12345678 | Go to: 192.168.4.1")

webpage = """<!DOCTYPE html>
<html>
<head>
    <title>Distance Sensor</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body {font-family: Arial; text-align: center; padding: 50px; background-color: #2c3e50; color: white;}
        .container {background-color: #34495e; padding: 40px; border-radius: 15px; max-width: 400px; margin: 0 auto;}
        h1 {font-size: 2em; margin-bottom: 30px;}
        .distance {font-size: 5em; font-weight: bold; color: #3498db;}
        .unit {font-size: 0.3em; color: #95a5a6;}
        .info {margin-top: 30px; font-size: 1.2em; color: #95a5a6;}
    </style>
    <script>
        function getDistance() {
            fetch('/distance').then(r => r.text()).then(d => {
                document.getElementById('distance').textContent = d;
            });
        }
        setInterval(getDistance, 500);
        getDistance();
    </script>
</head>
<body>
    <div class="container">
        <h1>Distance Sensor</h1>
        <div class="distance"><span id="distance">--</span><span class="unit">cm</span></div>
        <div class="info">Live updating</div>
    </div>
</body>
</html>"""

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
    if time.ticks_diff(time.ticks_ms(), last_time) > 100:
        d = measure_distance()
        if d > 0:
            current_distance = round(d, 1)
            print(f"Distance: {current_distance} cm")
        last_time = time.ticks_ms()

    try:
        conn, addr = s.accept()
        conn.settimeout(1.0)
        req = conn.recv(1024).decode()

        if 'GET /distance' in req:
            conn.send('HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\n')
            conn.sendall(f'{current_distance}')
        else:
            conn.send('HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n')
            conn.sendall(webpage)
        conn.close()
    except:
        pass

    time.sleep_ms(10)
