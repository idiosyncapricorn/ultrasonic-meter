////const int TRIG = 27;
////const int ECHO = 26;
////const int LED  = 2;
//
////void setup() {
////  Serial.begin(115200);
////  pinMode(TRIG, OUTPUT);
////  pinMode(ECHO, INPUT_PULLDOWN);
////  pinMode(LED, OUTPUT);
////  digitalWrite(TRIG, LOW);
////}
//
//void loop() {
////  digitalWrite(TRIG, HIGH);
////  delayMicroseconds(10);
////  digitalWrite(TRIG, LOW);
//
////  unsigned long t0 = micros();
////  while (digitalRead(ECHO) == LOW) {
////    if (micros() - t0 > 3000) break;
////  }
//
////  unsigned long t1 = micros();
////  while (digitalRead(ECHO) == HIGH) {
////    if (micros() - t1 > 30000) break;
////  }
//
////  unsigned long pulse = micros() - t1;
////  Serial.println(pulse);
//
//  ////if (pulse < 500) digitalWrite(LED, HIGH);
////  else digitalWrite(LED, LOW);
//
////  delay(1000);
////}
