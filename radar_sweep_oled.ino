// ============================================================================
// HC-SR04 + SERVO SWEEP + OLED DISPLAY — Arduino
// ACTIVE TRACKING: Angle-based micro-sweep + high-frequency pulses
// ============================================================================

#include <Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============================================================================
// OLED CONFIGURATION
// ============================================================================

#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ============================================================================
// HARDWARE PINS
// ============================================================================

const int TRIG_PIN  = 9;
const int ECHO_PIN  = 10;
const int LED_PIN   = 13;
const int SERVO_PIN = 6;

// ============================================================================
// DETECTION & TRACKING CONFIGURATION
// ============================================================================

const float ALERT_DISTANCE_CM      = 30.0;  // Perimeter radius (cm)
const float TRACK_DEAD_ZONE_CM     = 2.0;   // Ignore tiny distance noise

const unsigned long CLEAR_HOLDOFF_MS   = 1500;  // ms clear before sweep resumes
const unsigned long TRACKING_INTERVAL  = 20;    // ms between tracking pulses (50 Hz)
const unsigned long SWEEP_INTERVAL     = 50;    // ms between sweep steps
const unsigned long DISTANCE_INTERVAL  = 100;   // ms between normal distance checks
const unsigned long DISPLAY_INTERVAL   = 200;   // ms between OLED refreshes
const unsigned long PRINT_INTERVAL     = 500;   // ms between serial prints

// Sweep config
const int SWEEP_SPEED = 5;

// Angle-based tracking micro-sweep
const int TRACK_SWEEP_RANGE = 5;   // ±5 degrees around locked angle
const int TRACK_SWEEP_STEP  = 2;   // step size for micro-sweep

// ============================================================================
// GLOBAL STATE
// ============================================================================

Servo myServo;

float currentDistance  = 0.0;
float previousDistance = 0.0;
int   currentAngle     = 0;

// Sweep state
int sweepAngle     = 0;
int sweepDirection = 1;

// Detection / tracking state
enum RadarMode { SWEEPING, TRACKING };
RadarMode mode    = SWEEPING;
int   lockedAngle = 0;
unsigned long clearedAt = 0;

// Angle-based tracking internals
int   trackScanAngle     = 0;
bool  trackScanDirection = 1;   // 1 = right, 0 = left
float bestDistance       = 9999;
int   bestAngle          = 0;

// Timing
unsigned long lastDistanceTime  = 0;
unsigned long lastTrackingTime  = 0;
unsigned long lastSweepTime     = 0;
unsigned long lastPrintTime     = 0;
unsigned long lastDisplayTime   = 0;

// ============================================================================
// DISTANCE MEASUREMENT
// ============================================================================

float measureDistance() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    long duration = pulseIn(ECHO_PIN, HIGH, 30000UL);
    if (duration == 0) return -1.0;
    return (duration * 0.0343) / 2.0;
}

// ============================================================================
// HIGH-FREQUENCY TRACKING PULSE
// (just keeps distance updated + handles target loss)
// ============================================================================

void runTrackingPulse() {
    float d = measureDistance();
    if (d <= 0) return;

    previousDistance = currentDistance;
    currentDistance  = d;

    // If object has left the perimeter, start the clear holdoff
    bool objectDetected = (currentDistance > 0 && currentDistance <= ALERT_DISTANCE_CM);
    if (!objectDetected) {
        if (clearedAt == 0) clearedAt = millis();
    } else {
        clearedAt = 0;
    }
}

// ============================================================================
// ANGLE-BASED MICRO-SWEEP TRACKING
// ============================================================================

void runAngleTracking() {
    // Sweep left/right around locked angle
    if (trackScanDirection) {
        trackScanAngle += TRACK_SWEEP_STEP;
        if (trackScanAngle >= lockedAngle + TRACK_SWEEP_RANGE) {
            trackScanAngle = lockedAngle + TRACK_SWEEP_RANGE;
            trackScanDirection = 0;
        }
    } else {
        trackScanAngle -= TRACK_SWEEP_STEP;
        if (trackScanAngle <= lockedAngle - TRACK_SWEEP_RANGE) {
            trackScanAngle = lockedAngle - TRACK_SWEEP_RANGE;
            trackScanDirection = 1;
        }
    }

    trackScanAngle = constrain(trackScanAngle, 0, 180);
    myServo.write(trackScanAngle);
    currentAngle = trackScanAngle;
    delay(5);  // small settle time for servo + echo

    // Measure distance at this angle
    float d = measureDistance();
    if (d > 0 && d < bestDistance - TRACK_DEAD_ZONE_CM) {
        bestDistance = d;
        bestAngle    = trackScanAngle;
    }

    // When we hit either edge of the micro-sweep, move to best angle
    bool atRightEdge = (trackScanDirection == 1 && trackScanAngle == lockedAngle + TRACK_SWEEP_RANGE);
    bool atLeftEdge  = (trackScanDirection == 0 && trackScanAngle == lockedAngle - TRACK_SWEEP_RANGE);

    if (atRightEdge || atLeftEdge) {
        myServo.write(bestAngle);
        currentAngle = bestAngle;
        lockedAngle  = bestAngle;

        // Reset for next micro-sweep
        bestDistance = 9999;
        bestAngle    = lockedAngle;
    }
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
    if (mode == TRACKING) {
        display.println(F(">> TRACKING TARGET <<"));
    } else {
        display.println(F("RADAR SWEEP"));
    }
    display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);

    // Distance
    display.setTextSize(2);
    display.setCursor(0, 18);
    display.print(F("D:"));
    if (currentDistance > 0) {
        display.print(currentDistance, 1);
        display.println(F(" cm"));
    } else {
        display.println(F("--.- cm"));
    }

    // Angle
    display.setCursor(0, 38);
    display.print(F("A:"));
    display.print(currentAngle);
    if (mode == TRACKING) {
        display.println(F(" TRK"));
    } else {
        display.println(F(" deg"));
    }

    // Position bar
    display.setTextSize(1);
    display.setCursor(0,   56); display.print(F("0"));
    display.setCursor(60,  56); display.print(F("90"));
    display.setCursor(115, 56); display.print(F("180"));

    int markerX = map(currentAngle, 0, 180, 0, SCREEN_WIDTH - 1);

    if (mode == TRACKING) {
        display.drawFastVLine(markerX, 53, 9, SSD1306_WHITE);
        display.drawFastHLine(markerX - 3, 57, 7, SSD1306_WHITE);
    } else {
        display.fillCircle(markerX, 58, 2, SSD1306_WHITE);
    }

    display.display();
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    Serial.begin(115200);

    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;);
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(F("RADAR SYSTEM"));
    display.println(F("Initializing..."));
    display.display();
    delay(1000);

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    pinMode(LED_PIN,  OUTPUT);
    digitalWrite(TRIG_PIN, LOW);
    digitalWrite(LED_PIN,  LOW);

    myServo.attach(SERVO_PIN);
    myServo.write(0);
    delay(500);

    Serial.println(F("============================================================"));
    Serial.println(F("  HC-SR04 + SERVO SWEEP + OLED — Active Tracking Edition"));
    Serial.println(F("============================================================"));
    Serial.print  (F("  Alert perimeter  : ")); Serial.print(ALERT_DISTANCE_CM);   Serial.println(F(" cm"));
    Serial.print  (F("  Tracking rate    : ")); Serial.print(1000/TRACKING_INTERVAL); Serial.println(F(" Hz"));
    Serial.println(F("============================================================"));

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
    // TASK 1: Normal distance sensing + state transitions (SWEEPING)
    // ----------------------------------------------------------------
    if (mode == SWEEPING && (now - lastDistanceTime >= DISTANCE_INTERVAL)) {
        float d = measureDistance();
        if (d > 0) currentDistance = d;

        bool objectDetected = (currentDistance > 0 && currentDistance <= ALERT_DISTANCE_CM);

        if (objectDetected) {
            // Object entered perimeter — lock on + start tracking
            mode        = TRACKING;
            lockedAngle = currentAngle;

            trackScanAngle     = lockedAngle;
            trackScanDirection = 1;
            bestDistance       = 9999;
            bestAngle          = lockedAngle;
            clearedAt          = 0;

            myServo.write(lockedAngle);

            Serial.print(F(">> LOCK + TRACK — object at "));
            Serial.print(currentDistance, 1);
            Serial.print(F(" cm | angle "));
            Serial.print(lockedAngle);
            Serial.println(F(" deg"));
        }

        digitalWrite(LED_PIN, objectDetected ? HIGH : LOW);
        lastDistanceTime = now;
    }

    // ----------------------------------------------------------------
    // TASK 2: High-frequency tracking (TRACKING mode)
    // ----------------------------------------------------------------
    if (mode == TRACKING && (now - lastTrackingTime >= TRACKING_INTERVAL)) {
        runTrackingPulse();   // keep distance updated + loss detection
        runAngleTracking();   // micro-sweep around locked angle

        lastTrackingTime = now;

        // Check if hold-off has expired → resume sweep
        if (clearedAt != 0 && (now - clearedAt) >= CLEAR_HOLDOFF_MS) {
            mode       = SWEEPING;
            sweepAngle = currentAngle;
            clearedAt  = 0;
            Serial.println(F(">> Target lost — resuming sweep"));
        }

        bool objectDetected = (currentDistance > 0 && currentDistance <= ALERT_DISTANCE_CM);
        digitalWrite(LED_PIN, objectDetected ? HIGH : LOW);
    }

    // ----------------------------------------------------------------
    // TASK 3: Servo sweep (SWEEPING mode)
    // ----------------------------------------------------------------
    if (mode == SWEEPING && (now - lastSweepTime >= SWEEP_INTERVAL)) {
        updateSweep();
        lastSweepTime = now;
    }

    // ----------------------------------------------------------------
    // TASK 4: OLED display update
    // ----------------------------------------------------------------
    if (now - lastDisplayTime >= DISPLAY_INTERVAL) {
        updateDisplay();
        lastDisplayTime = now;
    }

    // ----------------------------------------------------------------
    // TASK 5: Serial status
    // ----------------------------------------------------------------
    if (now - lastPrintTime >= PRINT_INTERVAL) {
        if (mode == TRACKING) {
            Serial.print(F("[TRACKING] Dist: "));
            Serial.print(currentDistance, 1);
            Serial.print(F(" cm | Angle: "));
            Serial.print(currentAngle);
            Serial.println(F(" deg"));
        } else {
            Serial.print(F("[SWEEP]    Dist: "));
            Serial.print(currentDistance, 1);
            Serial.print(F(" cm | Angle: "));
            Serial.print(currentAngle);
            Serial.println(F(" deg"));
        }
        lastPrintTime = now;
    }
}
