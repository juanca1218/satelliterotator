#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>
#include <Adafruit_NeoPixel.h>

#define RELAY_ACTIVE_LEVEL   HIGH
#define RELAY_INACTIVE_LEVEL LOW

// LCD: address 0x27, 20×4
typedef LiquidCrystal_I2C LCD;
LCD lcd(0x27, 20, 4);

// BNO055 IMU
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28);

// Relay pins
const int relayAzLeft  = 13;
const int relayAzRight = 14;
const int relayElDown  = 21;
const int relayElUp    = 47;

// On-board NeoPixel LED (WS2812)
#define LED_PIN    48       // onboard LED data pin
#define NUM_LEDS   1
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

bool gpredictConnected = false;
String inputCommand = "";
unsigned long lastUpdateMs = 0;
const unsigned long updateInterval = 500; // ms

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(200);
  Wire.begin(10, 9);

  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("Initializing...");

  // Initialize IMU
  if (!bno.begin()) {
    lcd.setCursor(0,1);
    lcd.print("BNO055 not found!");
    while (1) delay(10);
  }
  delay(500);
  lcd.clear();
  lcd.print("BNO055 Ready!");

  // Sensor offsets
  adafruit_bno055_offsets_t offsets = {
    .accel_offset_x = 0,   .accel_offset_y = 0,    .accel_offset_z = -24,
    .mag_offset_x   = 236, .mag_offset_y   = -38,  .mag_offset_z   = -722,
    .gyro_offset_x  = 0,   .gyro_offset_y  = 2,    .gyro_offset_z  = 0,
    .accel_radius   = 1000, .mag_radius     = 734
  };
  bno.setSensorOffsets(offsets);
  bno.setExtCrystalUse(true);
  delay(200);

  // Configure relays
  pinMode(relayAzLeft,  OUTPUT);
  pinMode(relayAzRight, OUTPUT);
  pinMode(relayElUp,    OUTPUT);
  pinMode(relayElDown,  OUTPUT);
  digitalWrite(relayAzLeft,  RELAY_INACTIVE_LEVEL);
  digitalWrite(relayAzRight, RELAY_INACTIVE_LEVEL);
  digitalWrite(relayElUp,    RELAY_INACTIVE_LEVEL);
  digitalWrite(relayElDown,  RELAY_INACTIVE_LEVEL);

  // Initialize NeoPixel
  strip.begin();
  strip.show();  // all off

  lcd.clear();
  lcd.print("Ready to begin...");
}

void loop() {
  // Process incoming GS-232 commands on Serial
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r' || c == '\n') {
      if (inputCommand.length()) {
        inputCommand.trim();
        handleCommand(inputCommand);
        inputCommand = "";
      }
    } else {
      inputCommand += c;
    }
  }

  // Periodic display update
  unsigned long now = millis();
  if (now - lastUpdateMs >= updateInterval) {
    updateOrientationDisplay();
    lastUpdateMs = now;
  }
}

void updateOrientationDisplay() {
  sensors_event_t ev;
  bno.getEvent(&ev);
  float az = ev.orientation.x + 180.0;
  if (az >= 360.0) az -= 360.0;
  float el = ev.orientation.y + 47.0;

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("AZ: "); lcd.print(az, 1);
  lcd.setCursor(0,1);
  lcd.print("EL: "); lcd.print(el, 1);

  if (gpredictConnected) {
    lcd.setCursor(0,3);
    lcd.print("Connected!");
  }
}

void handleCommand(const String &cmd) {
  // Mark connection and light LED once
  if (!gpredictConnected) {
    gpredictConnected = true;
    strip.setPixelColor(0, strip.Color(0,100,0));
    strip.show();
  }

  if (cmd.startsWith("AZ")) {
    // Format: AZ <value>
    if (cmd.length() > 3 && isDigit(cmd.charAt(3))) {
      float target = cmd.substring(3).toFloat();
      // TODO: move logic here if needed
      Serial.print("RPRT 0\r");
    } else {
      // Report current
      sensors_event_t ev;
      bno.getEvent(&ev);
      float az = ev.orientation.x + 180.0;
      if (az >= 360.0) az -= 360.0;
      char buf[16];
      snprintf(buf, sizeof(buf), "AZ %.1f", az);
      Serial.print(buf); Serial.print('\r');
    }

  } else if (cmd.startsWith("EL")) {
    // Format: EL <value>
    if (cmd.length() > 3 && isDigit(cmd.charAt(3))) {
      float target = cmd.substring(3).toFloat();
      // TODO: move logic here if needed
      Serial.print("RPRT 0\r");
    } else {
      sensors_event_t ev;
      bno.getEvent(&ev);
      float el = ev.orientation.y + 47.0;
      char buf[16];
      snprintf(buf, sizeof(buf), "EL %.1f", el);
      Serial.print(buf); Serial.print('\r');
    }

  } else if (cmd == "SA") {
    digitalWrite(relayAzLeft, RELAY_INACTIVE_LEVEL);
    digitalWrite(relayAzRight, RELAY_INACTIVE_LEVEL);
    Serial.print("RPRT 0\r");

  } else if (cmd == "SE") {
    digitalWrite(relayElUp, RELAY_INACTIVE_LEVEL);
    digitalWrite(relayElDown, RELAY_INACTIVE_LEVEL);
    Serial.print("RPRT 0\r");

  } else if (cmd == "VE") {
    Serial.print("VEAdamESP32v1.0\r");

  } else if (cmd == "Q") {
    // Clear connection state
    gpredictConnected = false;
    strip.clear(); strip.show();

  } else if (cmd == "q") {
    // Clear connection state
    gpredictConnected = false;
    strip.clear(); strip.show();

  } else {
    Serial.print("RPRT -1\r");
  }
}
