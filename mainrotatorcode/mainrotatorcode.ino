#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>

// LCD address: 0x27, 20 cols, 4 rows
LiquidCrystal_I2C lcd(0x27, 20, 4);

// BNO055 I²C address: 0x28
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28);

static const float MOUNT_OFFSET = 49.0f; // Adjust for your pan-tilt mount

float lastBearing = 9999;
float lastElevation = 9999;
unsigned long lastUpdate = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin(10, 9);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("Initializing...");

  if (!bno.begin()) {
    Serial.println("BNO055 not detected!");
    lcd.setCursor(0,1);
    lcd.print("BNO055 not found!");
    while (1);
  }

  delay(1000);
  lcd.clear();
  lcd.print("BNO055 Ready!");
  Serial.println("BNO055 detected successfully!");

  bno.setExtCrystalUse(true);

  // ===== Load saved calibration offsets =====
  adafruit_bno055_offsets_t offsets = {
    .accel_offset_x = 0,
    .accel_offset_y = 0,
    .accel_offset_z = -24,
    .mag_offset_x   = 236,
    .mag_offset_y   = -38,
    .mag_offset_z   = -722,
    .gyro_offset_x  = 0,
    .gyro_offset_y  = 2,
    .gyro_offset_z  = 0,
    .accel_radius   = 1000,
    .mag_radius     = 734
  };
  bno.setSensorOffsets(offsets);

  delay(100); // Small delay after setting offsets
  lcd.clear();
}

void clearLine(int line) {
  lcd.setCursor(0, line);
  lcd.print("                    "); // 20 spaces to fully clear
}

void loop() {
  sensors_event_t orientation;
  bno.getEvent(&orientation);

  float bearing = orientation.orientation.x;
  float elevation = orientation.orientation.y + MOUNT_OFFSET;

  // === Flip North/South to match your dish orientation ===
  bearing += 180.0;
  if (bearing >= 360.0) {
    bearing -= 360.0;
  }

  if (millis() - lastUpdate > 500) {
    lastUpdate = millis();

    if (abs(bearing - lastBearing) > 0.5 || abs(elevation - lastElevation) > 0.5) {
      lastBearing = bearing;
      lastElevation = elevation;

      clearLine(0);
      clearLine(1);

      // Format bearing and elevation as strings
      String bearingStr = "Bearing: " + String(bearing, 1) + (char)223; // ° symbol
      String elevationStr = "Elevation: " + String(elevation, 1) + (char)223;

      // Calculate starting positions to center the text
      int bearingStart = (20 - bearingStr.length()) / 2;
      int elevationStart = (20 - elevationStr.length()) / 2;

      lcd.setCursor(bearingStart, 0);
      lcd.print(bearingStr);

      lcd.setCursor(elevationStart, 1);
      lcd.print(elevationStr);
    }
  }
}
