// ============================================================================
// HC-SR04 + SERVO SWEEP + OLED DISPLAY — Arduino
// Original: MicroPython on ESP32
// Converted: Arduino C++ with 1.2" OLED I2C Display
//
// Wiring:
//   HC-SR04 TRIG  → Pin 9
//   HC-SR04 ECHO  → Pin 10
//   Servo signal  → Pin 6  (PWM)
//   Alert LED     → Pin 13 (built-in LED on most boards)
//   OLED SDA      → Pin A4 (I2C)
//   OLED SCL      → Pin A5 (I2C)
//   OLED VCC      → 5V or 3.3V (check your module)
//   OLED GND      → GND
//   All GND       → GND
//   HC-SR04 VCC   → 5V
//   Servo VCC     → 5V (use external supply for heavy servos)
// ============================================================================

#include <Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============================================================================
// OLED CONFIGURATION
// ============================================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1      // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C   // Common I2C address (try 0x3D if this doesn't work)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ============================================================================
// HARDWARE PINS
// ============================================================================

const int TRIG_PIN  = 9;
const int ECHO_PIN  = 10;
const int LED_PIN   = 13;   // Built-in LED; lights up when object < 10 cm
const int SERVO_PIN = 6;    // Must be a PWM-capable pin

// ============================================================================
// GLOBAL STATE
// ============================================================================

Servo myServo;

float currentDistance = 0.0;
int   currentAngle    = 0;

// Sweep state
int sweepAngle     = 0;
int sweepDirection = 1;       // 1 = increasing, -1 = decreasing
const int SWEEP_SPEED = 5;    // Degrees per step (lower = smoother/slower)

// Timing (millis-based, non-blocking)
unsigned long lastDistanceTime = 0;
unsigned long lastSweepTime    = 0;
unsigned long lastPrintTime    = 0;
unsigned long lastDisplayTime  = 0;

const unsigned long DISTANCE_INTERVAL = 100;  // ms
const unsigned long SWEEP_INTERVAL    = 50;   // ms
const unsigned long PRINT_INTERVAL    = 1000; // ms
const unsigned long DISPLAY_INTERVAL  = 200;  // ms (update OLED 5x per second)

// ============================================================================
// DISTANCE MEASUREMENT
// ============================================================================

float measureDistance() {
    // Send a 10 µs trigger pulse
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    // Measure echo pulse width (timeout = 30 ms → ~5 m max range)
    long duration = pulseIn(ECHO_PIN, HIGH, 30000UL);

    if (duration == 0) {
        return -1.0;  // Timeout / no echo
    }

    // Distance in cm: sound travels 0.0343 cm/µs, divide by 2 for one-way
    float distance = (duration * 0.0343) / 2.0;
    return distance;
}

// ============================================================================
// SERVO SWEEP
// ============================================================================

void updateSweep() {
    sweepAngle += SWEEP_SPEED * sweepDirection;

    if (sweepAngle >= 180) {
        sweepAngle     = 180;
        sweepDirection = -1;
    } else if (sweepAngle <= 0) {
        sweepAngle     = 0;
        sweepDirection = 1;
    }

    myServo.write(sweepAngle);
    currentAngle = sweepAngle;
}

// ============================================================================
// OLED DISPLAY UPDATE
// ============================================================================

void updateDisplay() {
    display.clearDisplay();
    
    // Title
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(F("RADAR SWEEP"));
    display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);
    
    // Distance (large font)
    display.setTextSize(2);
    display.setCursor(0, 18);
    display.print(F("D:"));
    if (currentDistance > 0) {
        display.print(currentDistance, 1);
        display.println(F(" cm"));
    } else {
        display.println(F("--.- cm"));
    }
    
    // Servo angle (large font)
    display.setCursor(0, 38);
    display.print(F("A:"));
    display.print(currentAngle);
    display.println(F(" deg"));
    
    // Visual servo indicator (bottom bar)
    display.setTextSize(1);
    display.setCursor(0, 56);
    display.print(F("0"));
    display.setCursor(60, 56);
    display.print(F("90"));
    display.setCursor(115, 56);
    display.print(F("180"));
    
    // Draw servo position marker
    int markerX = map(currentAngle, 0, 180, 0, SCREEN_WIDTH - 1);
    display.fillCircle(markerX, 58, 2, SSD1306_WHITE);
    
    display.display();
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    Serial.begin(115200);

    // Initialize OLED
    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed"));
        for(;;); // Halt if OLED init fails
    }
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(F("RADAR SYSTEM"));
    display.println(F("Initializing..."));
    display.display();
    delay(1000);

    // Initialize hardware pins
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    pinMode(LED_PIN,  OUTPUT);

    digitalWrite(TRIG_PIN, LOW);
    digitalWrite(LED_PIN,  LOW);

    myServo.attach(SERVO_PIN);
    myServo.write(0);   // Start at 0°
    delay(500);

    Serial.println();
    Serial.println(F("============================================================"));
    Serial.println(F("  HC-SR04 + SERVO SWEEP + OLED — Arduino"));
    Serial.println(F("============================================================"));
    Serial.println(F("  Servo sweeps 0°-180° continuously"));
    Serial.println(F("  LED lights when object detected under 10 cm"));
    Serial.println(F("  OLED displays distance and angle in real-time"));
    Serial.println(F("============================================================"));
    Serial.println(F("Starting..."));
    Serial.println();
    
    // Show ready message on OLED
    display.clearDisplay();
    display.setCursor(0, 20);
    display.setTextSize(2);
    display.println(F("READY!"));
    display.display();
    delay(1000);
}

// ============================================================================
// MAIN LOOP — NON-BLOCKING, INTERVAL-BASED
// ============================================================================

void loop() {
    unsigned long now = millis();

    // ----------------------------------------------------------------
    // TASK 1: Distance sensing (every 100 ms)
    // ----------------------------------------------------------------
    if (now - lastDistanceTime >= DISTANCE_INTERVAL) {
        float d = measureDistance();
        if (d > 0) {
            currentDistance = d;
        }
        digitalWrite(LED_PIN, (currentDistance > 0 && currentDistance < 10) ? HIGH : LOW);
        lastDistanceTime = now;
    }

    // ----------------------------------------------------------------
    // TASK 2: Servo sweep (every 50 ms)
    // ----------------------------------------------------------------
    if (now - lastSweepTime >= SWEEP_INTERVAL) {
        updateSweep();
        lastSweepTime = now;
    }

    // ----------------------------------------------------------------
    // TASK 3: OLED display update (every 200 ms)
    // ----------------------------------------------------------------
    if (now - lastDisplayTime >= DISPLAY_INTERVAL) {
        updateDisplay();
        lastDisplayTime = now;
    }

    // ----------------------------------------------------------------
    // TASK 4: Serial status print (every 1 second)
    // ----------------------------------------------------------------
    if (now - lastPrintTime >= PRINT_INTERVAL) {
        Serial.print(F("Distance: "));
        Serial.print(currentDistance, 1);
        Serial.print(F(" cm | Servo: "));
        Serial.print(currentAngle);
        Serial.println(F(" deg"));
        lastPrintTime = now;
    }
}
