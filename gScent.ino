#include <Servo.h>

Servo servoLat;
Servo servoLon;

void setup() {
  Serial.begin(115200); // Match this speed in your C++ code
  servoLat.attach(9);   // Connect Lat Servo signal to Pin 9
  servoLon.attach(10);  // Connect Lon Servo signal to Pin 10
  
  // Start at center
  servoLat.write(90);
  servoLon.write(90);
}

byte currentLat = 90;
byte currentLon = 90;
const int deadzone = 5; // Your 12-degree "tolerance"

void loop() {
  if (Serial.available() >= 4) {
    if (Serial.read() == '<') {
      byte targetLat = Serial.read();
      byte targetLon = Serial.read();
      
      if (Serial.read() == '>') {
        
        // Only move if the change is BIGGER than the deadzone
        if (abs(targetLat - currentLat) > deadzone) {
          servoLat.write(targetLat);
          currentLat = targetLat;
        }
        
        if (abs(targetLon - currentLon) > deadzone) {
          servoLon.write(targetLon);
          currentLon = targetLon;
        }
      }
    }
  }
}