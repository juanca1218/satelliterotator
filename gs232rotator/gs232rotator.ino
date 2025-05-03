#include <Wire.h>
#include <EEPROM.h>
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

// NeoPixel indicator
#define LED_PIN    48
#define NUM_LEDS   1
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// GS-232 state
double targetAz = NAN, targetEl = NAN;
const float tolAz = 2.0f, tolEl = 2.0f;
bool azMove = false, elMove = false;
bool gpredictConnected = false;
String inputCommand;
unsigned long lastUpdateMs = 0;
const unsigned long updateInterval = 500;

// Elevation smoothing
#define SMOOTH_WINDOW 5
float elHistory[SMOOTH_WINDOW] = {0};
int elIndex = 0;
void pushEl(float v) { elHistory[elIndex] = v; elIndex = (elIndex+1) % SMOOTH_WINDOW; }
float getSmoothedEl() { float sum = 0; for (int i = 0; i < SMOOTH_WINDOW; i++) sum += elHistory[i]; return sum / SMOOTH_WINDOW; }

// Convert BNO055 heading (-180..180) -> 0..360 + offset
float readHeading(const sensors_event_t &ev) {
  float h = ev.orientation.x;
  if (h < 0) h += 360;
  h += 90; // mount offset
  if (h >= 360) h -= 360;
  return h;
}

// ——— Hard-coded calibration (optional) ———
const bool USE_HARDCODED = false;
const adafruit_bno055_offsets_t defaultOffsets = { 7, -67, -21, 0, 2, 0, 265, -85, -700, 1000, 556 };

// ——— EEPROM calibration storage ———
const uint32_t EEPROM_MAGIC  = 0x424E4F43;  // 'B' 'N' 'O' 'C'
const int      MAGIC_ADDR    = 0;
const int      OFFSETS_ADDR  = MAGIC_ADDR + sizeof(EEPROM_MAGIC);

// Calibration routines
void applyOffsets(const adafruit_bno055_offsets_t &o) {
  bno.setSensorOffsets(o);
  EEPROM.put(MAGIC_ADDR, EEPROM_MAGIC);
  EEPROM.put(OFFSETS_ADDR, o);
  EEPROM.commit();
}

bool loadEepromOffsets(adafruit_bno055_offsets_t &o) {
  uint32_t m;
  EEPROM.get(MAGIC_ADDR, m);
  if (m != EEPROM_MAGIC) return false;
  EEPROM.get(OFFSETS_ADDR, o);
  bno.setSensorOffsets(o);
  return true;
}

void printCalStats() {
  adafruit_bno055_offsets_t o;
  bno.getSensorOffsets(o);
  Serial.println("=== CALIBRATION OFFSETS ===");
  Serial.print("Accel Off: "); Serial.print(o.accel_offset_x); Serial.print(','); Serial.print(o.accel_offset_y); Serial.print(','); Serial.println(o.accel_offset_z);
  Serial.print("Mag   Off: "); Serial.print(o.mag_offset_x); Serial.print(','); Serial.print(o.mag_offset_y); Serial.print(','); Serial.println(o.mag_offset_z);
  Serial.print("Gyro  Off: "); Serial.print(o.gyro_offset_x); Serial.print(','); Serial.print(o.gyro_offset_y); Serial.print(','); Serial.println(o.gyro_offset_z);
  Serial.print("Accel Rad: "); Serial.println(o.accel_radius);
  Serial.print("Mag   Rad: "); Serial.println(o.mag_radius);
  Serial.println("===========================");
}

void calibrateAndStore() {
  Serial.println("🔄 CALIBRATION MODE — rotate until 3/3/3/3");
  uint8_t sys, gyr, acc, mag;
  do {
    bno.getCalibration(&sys, &gyr, &acc, &mag);
    Serial.print("CALIB: SYS="); Serial.print(sys);
    Serial.print(" GYR="); Serial.print(gyr);
    Serial.print(" ACC="); Serial.print(acc);
    Serial.print(" MAG="); Serial.println(mag);
    delay(500);
  } while (sys < 3 || gyr < 3 || acc < 3 || mag < 3);

  Serial.println("🎉 Calibrated — fetching offsets");
  adafruit_bno055_offsets_t o;
  bno.getSensorOffsets(o);
  applyOffsets(o);
  Serial.println("💾 Offsets written to EEPROM");
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(512);
  Serial.setTimeout(200);
  Wire.begin(10, 9);

  lcd.init(); lcd.backlight();
  lcd.clear(); lcd.print("Initializing...");

  if (!bno.begin()) {
    lcd.clear(); lcd.print("BNO055 FAIL!");
    while (1);
  }
  delay(100);

  // 1) Hard-coded?
  if (USE_HARDCODED) {
    Serial.println("Using hard-coded offsets");
    bno.setSensorOffsets(defaultOffsets);
  }
  // 2) EEPROM?
  else {
    adafruit_bno055_offsets_t eoffs;
    if (loadEepromOffsets(eoffs)) {
      Serial.println("Loaded offsets from EEPROM");
    } else {
      Serial.println("No stored offsets → calibrating now");
      calibrateAndStore();
    }
  }

  delay(50);
  bno.setExtCrystalUse(true);
  delay(500);
  sensors_event_t discard; bno.getEvent(&discard);

  lcd.clear(); lcd.print("BNO055 NDOF");

  pinMode(relayAzLeft, OUTPUT); pinMode(relayAzRight, OUTPUT);
  pinMode(relayElUp, OUTPUT);    pinMode(relayElDown, OUTPUT);
  digitalWrite(relayAzLeft, RELAY_INACTIVE_LEVEL);
  digitalWrite(relayAzRight, RELAY_INACTIVE_LEVEL);
  digitalWrite(relayElUp, RELAY_INACTIVE_LEVEL);
  digitalWrite(relayElDown, RELAY_INACTIVE_LEVEL);

  strip.begin(); strip.show();

  lcd.clear(); lcd.print("Self-test...");
  const int tms = 300;
  lcd.clear(); lcd.print("UP");    digitalWrite(relayElUp, HIGH); delay(tms); digitalWrite(relayElUp, LOW); delay(100);
  lcd.clear(); lcd.print("DOWN");  digitalWrite(relayElDown, HIGH); delay(tms); digitalWrite(relayElDown, LOW); delay(100);
  lcd.clear(); lcd.print("LEFT");  digitalWrite(relayAzLeft, HIGH); delay(tms); digitalWrite(relayAzLeft, LOW); delay(100);
  lcd.clear(); lcd.print("RIGHT"); digitalWrite(relayAzRight, HIGH); delay(tms); digitalWrite(relayAzRight, LOW); delay(100);

  lcd.clear(); lcd.print("Ready");
  delay(150);
}

void updateMovement() {
  sensors_event_t ev; bno.getEvent(&ev);
  float currAz = readHeading(ev);
  float rawEl  = ev.orientation.y + 47.0f;
  pushEl(rawEl);
  float currEl = getSmoothedEl();

  if (azMove) {
    float diff = targetAz - currAz;
    if (diff > 180) diff -= 360;
    else if (diff < -180) diff += 360;
    if (fabs(diff) > tolAz) {
      digitalWrite(relayAzRight, diff > 0 ? RELAY_ACTIVE_LEVEL : RELAY_INACTIVE_LEVEL);
      digitalWrite(relayAzLeft,  diff < 0 ? RELAY_ACTIVE_LEVEL : RELAY_INACTIVE_LEVEL);
    } else {
      digitalWrite(relayAzLeft, RELAY_INACTIVE_LEVEL);
      digitalWrite(relayAzRight, RELAY_INACTIVE_LEVEL);
      azMove = false;
    }
  }

  if (elMove) {
    float diff2 = targetEl - currEl;
    if (fabs(diff2) > tolEl) {
      digitalWrite(relayElUp,   diff2 > 0 ? RELAY_ACTIVE_LEVEL : RELAY_INACTIVE_LEVEL);
      digitalWrite(relayElDown, diff2 < 0 ? RELAY_ACTIVE_LEVEL : RELAY_INACTIVE_LEVEL);
    } else {
      digitalWrite(relayElUp,    RELAY_INACTIVE_LEVEL);
      digitalWrite(relayElDown,  RELAY_INACTIVE_LEVEL);
      elMove = false;
    }
  }
}

void updateOrientationDisplay() {
  uint8_t sys, gyro, accel, mag;
  bno.getCalibration(&sys, &gyro, &accel, &mag);
  sensors_event_t ev; bno.getEvent(&ev);
  float az = readHeading(ev);
  pushEl(ev.orientation.y + 47.0f);
  float el = getSmoothedEl();
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("AZ:"); lcd.print(az,1);
  lcd.setCursor(0,1); lcd.print("EL:"); lcd.print(el,1);
  lcd.setCursor(10,0); lcd.print("S:"); lcd.print(sys);
  lcd.setCursor(10,1); lcd.print("G:"); lcd.print(gyro);
  lcd.setCursor(10,2); lcd.print("A:"); lcd.print(accel);
  lcd.setCursor(10,3); lcd.print("M:"); lcd.print(mag);
  if (gpredictConnected) { lcd.setCursor(0,3); lcd.print("Connected"); }
  updateMovement();
}

void handleCommand(const String &cmd) {
  if (!gpredictConnected) {
    gpredictConnected = true;
    strip.setPixelColor(0, strip.Color(0,100,0)); strip.show();
  }
  if (cmd.startsWith("AZ")) {
    if (cmd.length()>3 && isDigit(cmd.charAt(3))) {
      targetAz = cmd.substring(3).toFloat(); azMove = true; Serial.print("RPRT 0\r");
    } else {
      sensors_event_t ev; bno.getEvent(&ev);
      char buf[16]; snprintf(buf, sizeof(buf), "AZ %.1f", readHeading(ev));
      Serial.print(buf); Serial.print("\r");
    }
  } else if (cmd.startsWith("EL")) {
    if (cmd.length()>3 && isDigit(cmd.charAt(3))) {
      targetEl = cmd.substring(3).toFloat(); elMove = true; Serial.print("RPRT 0\r");
    } else {
      sensors_event_t ev; bno.getEvent(&ev); float raw = ev.orientation.y + 47.0f; pushEl(raw);
      char buf[16]; snprintf(buf, sizeof(buf), "EL %.1f", getSmoothedEl());
      Serial.print(buf); Serial.print("\r");
    }
  } else if (cmd.equalsIgnoreCase("P")) {
    sensors_event_t ev; bno.getEvent(&ev);
    float heading = readHeading(ev); float raw = ev.orientation.y + 47.0f; pushEl(raw);
    char buf[32]; snprintf(buf, sizeof(buf), "%.1f %.1f", heading, getSmoothedEl());
    Serial.print(buf); Serial.print("\r");
  } else if (cmd.equalsIgnoreCase("CALSTATS")) {
    printCalStats(); Serial.print("RPRT 0\r");
  } else if (cmd.equalsIgnoreCase("CALIBRATE")) {
    Serial.println("Re-running calibration..."); calibrateAndStore(); Serial.print("RPRT 0\r");
  } else if (cmd.equalsIgnoreCase("SA")) {
    digitalWrite(relayAzLeft, RELAY_INACTIVE_LEVEL); digitalWrite(relayAzRight, RELAY_INACTIVE_LEVEL); Serial.print("RPRT 0\r");
  } else if (cmd.equalsIgnoreCase("SE")) {
    digitalWrite(relayElUp, RELAY_INACTIVE_LEVEL); digitalWrite(relayElDown, RELAY_INACTIVE_LEVEL); Serial.print("RPRT 0\r");
  } else if (cmd.equalsIgnoreCase("VE")) {
    Serial.print("VEAdamESP32v1.0\r");
  } else if (cmd.equalsIgnoreCase("Q")) {
    gpredictConnected = false; strip.clear(); strip.show();
  } else {
    Serial.print("RPRT -1\r");
  }
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c=='\r' || c=='\n') {
      if (inputCommand.length()) { inputCommand.trim(); handleCommand(inputCommand); inputCommand=""; }
    } else inputCommand += c;
  }
  if (millis() - lastUpdateMs >= updateInterval) { updateOrientationDisplay(); lastUpdateMs = millis(); }
}
