// Define pin numbers
const int TRIG = 27;   // TRIG connected to GPIO27
const int ECHO = 26;   // ECHO connected to GPIO26 (via divider)
const int LED  = 2;    // ESP32 onboard LED

void setup() {
  Serial.begin(115200);
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT_PULLDOWN);
  pinMode(LED, OUTPUT);
  digitalWrite(TRIG, LOW);
}

void loop() {
  // single 10 µs trigger
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  // wait for echo start (timeout ~3 ms)
  unsigned long t0 = micros();
  while (digitalRead(ECHO) == LOW) {
    if (micros() - t0 > 3000) { 
      Serial.println("Timeout: no echo start"); 
      break; 
    }
  }

  // measure echo high duration (timeout ~30 ms)
  unsigned long t1 = micros();
  while (digitalRead(ECHO) == HIGH) {
    if (micros() - t1 > 30000) { 
      Serial.println("Timeout: echo too long"); 
      break; 
    }
  }

  unsigned long pulse = micros() - t1;
  Serial.print("Echo us: "); 
  Serial.println(pulse);

  // 🔴 LED ON if distance < 500, OFF otherwise
  if (pulse < 500) {
    digitalWrite(LED, HIGH);
  } else {
    digitalWrite(LED, LOW);
  }

  delay(1000);
}
