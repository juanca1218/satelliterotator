#include <Wire.h>

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22); // SDA, SCL
  delay(1000);
  Serial.println("I2C Scanner starting...");
}

void loop() {
  byte count = 0;
  Serial.println("Scanning...");

  for (byte i = 8; i < 120; i++) {
    Wire.beginTransmission(i);
    if (Wire.endTransmission() == 0) {
      Serial.print("Found device at 0x");
      Serial.println(i, HEX);
      count++;
      delay(10);
    }
  }

  Serial.print("Done. Devices found: ");
  Serial.println(count);
  delay(5000);
}
