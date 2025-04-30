#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>

#define RELAY_ACTIVE_LEVEL HIGH
#define RELAY_INACTIVE_LEVEL LOW

LiquidCrystal_I2C lcd(0x27, 20, 4);
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28);
static const float MOUNT_OFFSET = 47.0f;

const int relayAzLeft = 13;
const int relayAzRight = 14;
const int relayElDown = 21;
const int relayElUp = 47;

const float bearingTolerance = 1.5;
const float elevationTolerance = 1.5;

float targetBearing = 180.0;
float targetElevation = 45.0;

#define SMOOTHING_WINDOW 5
float headingHistory[SMOOTHING_WINDOW] = {0};
int headingIndex = 0;

const unsigned long pulseDuration = 300;
unsigned long lastPulseTime = 0;
bool isPulsing = false;
unsigned long azLockoutTime = 0;

const unsigned long elPulseDuration = 300;
unsigned long lastElPulseTime = 0;
bool isElPulsing = false;
unsigned long elLockoutTime = 0;

String inputString = "";
bool stringComplete = false;

void setup() {
  Serial.begin(115200);
  Wire.begin(10, 9);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("Initializing...");

  if (!bno.begin()) {
    Serial.println("BNO055 not detected!");
    lcd.setCursor(0, 1);
    lcd.print("BNO055 not found!");
    while (1);
  }

  delay(1000);
  lcd.clear();
  lcd.print("BNO055 Ready!");
  Serial.println("BNO055 detected successfully!");

  bno.setExtCrystalUse(true);
  bno.setMode(OPERATION_MODE_NDOF);
  delay(10);

  adafruit_bno055_offsets_t offsets = {
    .accel_offset_x = 0,
    .accel_offset_y = 0,
    .accel_offset_z = -24,
    .mag_offset_x = 236,
    .mag_offset_y = -38,
    .mag_offset_z = -722,
    .gyro_offset_x = 0,
    .gyro_offset_y = 2,
    .gyro_offset_z = 0,
    .accel_radius = 1000,
    .mag_radius = 734
  };
  bno.setSensorOffsets(offsets);
  delay(100);
  lcd.clear();

  pinMode(relayAzLeft, OUTPUT);
  pinMode(relayAzRight, OUTPUT);
  pinMode(relayElUp, OUTPUT);
  pinMode(relayElDown, OUTPUT);

  digitalWrite(relayAzLeft, RELAY_INACTIVE_LEVEL);
  digitalWrite(relayAzRight, RELAY_INACTIVE_LEVEL);
  digitalWrite(relayElUp, RELAY_INACTIVE_LEVEL);
  digitalWrite(relayElDown, RELAY_INACTIVE_LEVEL);

  lcd.setCursor(1, 0);
  lcd.print("Homing to AZ180");
  lcd.setCursor(2, 1);
  lcd.print("and EL45...");
  for (int i = 0; i < 3; i++) {
    lcd.setCursor(18 - i, 1);
    lcd.print(".");
    delay(500);
  }
  delay(1000);
  lcd.clear();
}

void clearLine(int line) {
  lcd.setCursor(0, line);
  lcd.print("                    ");
}

void stopAllRelays() {
  digitalWrite(relayAzLeft, RELAY_INACTIVE_LEVEL);
  digitalWrite(relayAzRight, RELAY_INACTIVE_LEVEL);
  digitalWrite(relayElUp, RELAY_INACTIVE_LEVEL);
  digitalWrite(relayElDown, RELAY_INACTIVE_LEVEL);
  isPulsing = false;
  isElPulsing = false;
}

void processCommand(String cmd) {
  cmd.trim();
  cmd.toUpperCase();

  if (cmd.startsWith("AZ")) {
    float newAz = cmd.substring(2).toFloat();
    if (newAz >= 0.0 && newAz <= 360.0) {
      targetBearing = newAz;
      Serial.print("New target bearing: ");
      Serial.println(targetBearing);
    }
  } else if (cmd.startsWith("EL")) {
    float newEl = cmd.substring(2).toFloat();
    if (newEl >= 0.0 && newEl <= 90.0) {
      targetElevation = newEl;
      Serial.print("New target elevation: ");
      Serial.println(targetElevation);
    }
  } else if (cmd == "STOP") {
    stopAllRelays();
    Serial.println("All relays stopped.");
  } else if (cmd == "STATUS") {
    Serial.print("Current Bearing: ");
    Serial.println(targetBearing);
    Serial.print("Current Elevation: ");
    Serial.println(targetElevation);
  } else {
    Serial.println("Unknown command. Use AZ###, EL###, STOP, or STATUS");
  }
}

void loop() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    if (inChar == '\n' || inChar == '\r') {
      if (inputString.length() > 0) {
        stringComplete = true;
      }
    } else {
      inputString += inChar;
    }
  }

  if (stringComplete) {
    processCommand(inputString);
    inputString = "";
    stringComplete = false;
  }

  sensors_event_t orientation;
  bno.getEvent(&orientation);

  float rawBearing = orientation.orientation.x;
  float elevation = orientation.orientation.y + MOUNT_OFFSET;

  rawBearing += 180.0;
  if (rawBearing >= 360.0) rawBearing -= 360.0;

  headingHistory[headingIndex] = rawBearing;
  headingIndex = (headingIndex + 1) % SMOOTHING_WINDOW;

  float smoothedBearing = 0;
  for (int i = 0; i < SMOOTHING_WINDOW; i++) {
    smoothedBearing += headingHistory[i];
  }
  smoothedBearing /= SMOOTHING_WINDOW;

  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 500) {
    lastUpdate = millis();
    clearLine(0);
    clearLine(1);

    String bearingStr = "Bearing: " + String(smoothedBearing, 1) + (char)223;
    String elevationStr = "Elevation: " + String(elevation, 1) + (char)223;

    lcd.setCursor((20 - bearingStr.length()) / 2, 0);
    lcd.print(bearingStr);
    lcd.setCursor((20 - elevationStr.length()) / 2, 1);
    lcd.print(elevationStr);
  }

  if (!isPulsing && millis() - azLockoutTime > 500 && abs(smoothedBearing - targetBearing) > bearingTolerance) {
    if ((smoothedBearing > targetBearing && (smoothedBearing - targetBearing) < 180) ||
        (smoothedBearing < targetBearing && (targetBearing - smoothedBearing) > 180)) {
      digitalWrite(relayAzLeft, RELAY_ACTIVE_LEVEL);
      digitalWrite(relayAzRight, RELAY_INACTIVE_LEVEL);
    } else {
      digitalWrite(relayAzLeft, RELAY_INACTIVE_LEVEL);
      digitalWrite(relayAzRight, RELAY_ACTIVE_LEVEL);
    }
    isPulsing = true;
    lastPulseTime = millis();
  }

  if (isPulsing && millis() - lastPulseTime > pulseDuration) {
    digitalWrite(relayAzLeft, RELAY_INACTIVE_LEVEL);
    digitalWrite(relayAzRight, RELAY_INACTIVE_LEVEL);
    delay(200);
    isPulsing = false;
    azLockoutTime = millis();
  }

  if (!isElPulsing && millis() - elLockoutTime > 500 && abs(elevation - targetElevation) > elevationTolerance) {
    if (elevation < targetElevation) {
      digitalWrite(relayElUp, RELAY_ACTIVE_LEVEL);
      digitalWrite(relayElDown, RELAY_INACTIVE_LEVEL);
    } else {
      digitalWrite(relayElUp, RELAY_INACTIVE_LEVEL);
      digitalWrite(relayElDown, RELAY_ACTIVE_LEVEL);
    }
    isElPulsing = true;
    lastElPulseTime = millis();
  }

  if (isElPulsing && millis() - lastElPulseTime > elPulseDuration) {
    digitalWrite(relayElUp, RELAY_INACTIVE_LEVEL);
    digitalWrite(relayElDown, RELAY_INACTIVE_LEVEL);
    delay(200);
    isElPulsing = false;
    elLockoutTime = millis();
  }
}