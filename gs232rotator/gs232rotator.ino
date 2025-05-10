#include <Wire.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>
#include <Adafruit_NeoPixel.h>

#define RELAY_ACTIVE_LEVEL   HIGH
#define RELAY_INACTIVE_LEVEL LOW

// —— OLED Setup ——
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

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
  h += 90;
  if (h >= 360) h -= 360;
  return h;
}

// ——— Hard-coded calibration (optional) ———

const bool USE_HARDCODED = true;
const adafruit_bno055_offsets_t defaultOffsets = { -32, -59, -32, 216, -63, -654, -1, 2, 0, 1000, 746 };

// ——— EEPROM calibration storage ———
const uint32_t EEPROM_MAGIC  = 0x424E4F43;
const int      MAGIC_ADDR    = 0;
const int      OFFSETS_ADDR  = MAGIC_ADDR + sizeof(EEPROM_MAGIC);

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
  Serial.print("Mag   Off: ");   Serial.print(o.mag_offset_x);   Serial.print(','); Serial.print(o.mag_offset_y);   Serial.print(','); Serial.println(o.mag_offset_z);
  Serial.print("Gyro  Off: ");  Serial.print(o.gyro_offset_x);  Serial.print(','); Serial.print(o.gyro_offset_y);  Serial.print(','); Serial.println(o.gyro_offset_z);
  Serial.print("Accel Rad: "); Serial.println(o.accel_radius);
  Serial.print("Mag   Rad: ");   Serial.println(o.mag_radius);
  Serial.println("===========================");
}

// ——— Calibration with on-screen & LED feedback ———
void calibrateAndStore() {
  Serial.println("🔄 CALIBRATION MODE — rotate until 3/3/3/3");
  uint8_t sys=0, gyr=0, acc=0, mag=0;

  while (sys < 3 || gyr < 3 || acc < 3 || mag < 3) {
    bno.getCalibration(&sys, &gyr, &acc, &mag);

    // Serial
    Serial.print("CALIB: SYS="); Serial.print(sys);
    Serial.print(" GYR=");    Serial.print(gyr);
    Serial.print(" ACC=");    Serial.print(acc);
    Serial.print(" MAG=");    Serial.println(mag);

    // Flash NeoPixel Yellow
    strip.setPixelColor(0, strip.Color(255, 255, 0));
    strip.show(); delay(200);
    strip.clear();
    strip.show(); delay(200);

    // OLED
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("CALIBRATION MODE");
    display.setCursor(0, 12);  display.print("SYS="); display.print(sys);
    display.setCursor(64,12);  display.print("GYR="); display.print(gyr);
    display.setCursor(0, 24);  display.print("ACC="); display.print(acc);
    display.setCursor(64,24);  display.print("MAG="); display.print(mag);
    display.display();

    delay(300);
  }

  // Fetch & store
  Serial.println("🎉 Calibrated — fetching offsets");
  adafruit_bno055_offsets_t o;
  bno.getSensorOffsets(o);
  applyOffsets(o);
  Serial.println("💾 Offsets written to EEPROM");

  // Print initializer
  Serial.println("Use these for defaultOffsets:");
  Serial.print("const adafruit_bno055_offsets_t defaultOffsets = { ");
  Serial.print(o.accel_offset_x); Serial.print(", ");
  Serial.print(o.accel_offset_y); Serial.print(", ");
  Serial.print(o.accel_offset_z); Serial.print(", ");
  Serial.print(o.mag_offset_x);   Serial.print(", ");
  Serial.print(o.mag_offset_y);   Serial.print(", ");
  Serial.print(o.mag_offset_z);   Serial.print(", ");
  Serial.print(o.gyro_offset_x);  Serial.print(", ");
  Serial.print(o.gyro_offset_y);  Serial.print(", ");
  Serial.print(o.gyro_offset_z);  Serial.print(", ");
  Serial.print(o.accel_radius);   Serial.print(", ");
  Serial.print(o.mag_radius);     Serial.println(" };");

  // OLED “Done”
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("CALIBRATION DONE!");
  display.display();
  delay(1000);
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(512);
  Wire.begin(10, 9);

  // NeoPixel first
  strip.begin();
  strip.setBrightness(50);
  strip.clear();
  strip.show();

  // OLED init
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 failed");
    for (;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("Initializing...");
  display.display();

  // BNO055 init
  if (!bno.begin()) {
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("BNO055 FAIL!");
    display.display();
    while (1);
  }
  delay(100);

  // Calibration choice
  if (USE_HARDCODED) {
    Serial.println("Using hard-coded offsets");
    bno.setSensorOffsets(defaultOffsets);
  } else {
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

  // Self-test
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("BNO055 NDOF");
  display.display();

  pinMode(relayAzLeft, OUTPUT);
  pinMode(relayAzRight, OUTPUT);
  pinMode(relayElUp, OUTPUT);
  pinMode(relayElDown, OUTPUT);
  digitalWrite(relayAzLeft, RELAY_INACTIVE_LEVEL);
  digitalWrite(relayAzRight, RELAY_INACTIVE_LEVEL);
  digitalWrite(relayElUp, RELAY_INACTIVE_LEVEL);
  digitalWrite(relayElDown, RELAY_INACTIVE_LEVEL);

  const int tms = 300;
  display.clearDisplay(); display.setCursor(0,0); display.println("UP");    display.display();
  digitalWrite(relayElUp, HIGH);    delay(tms); digitalWrite(relayElUp, LOW);    delay(100);
  display.clearDisplay(); display.setCursor(0,0); display.println("DOWN");  display.display();
  digitalWrite(relayElDown, HIGH);  delay(tms); digitalWrite(relayElDown, LOW);  delay(100);
  display.clearDisplay(); display.setCursor(0,0); display.println("LEFT");  display.display();
  digitalWrite(relayAzLeft, HIGH);  delay(tms); digitalWrite(relayAzLeft, LOW);  delay(100);
  display.clearDisplay(); display.setCursor(0,0); display.println("RIGHT"); display.display();
  digitalWrite(relayAzRight, HIGH); delay(tms); digitalWrite(relayAzRight, LOW); delay(100);

  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Ready");
  display.display();
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
  static bool blinkState = false;
  uint8_t sys, gyro, accel, mag;
  bno.getCalibration(&sys, &gyro, &accel, &mag);
  sensors_event_t ev; bno.getEvent(&ev);
  float az = readHeading(ev);
  pushEl(ev.orientation.y + 47.0f);
  float el = getSmoothedEl();

  // Blink yellow if sys < 3
  if (sys < 3) {
    blinkState = !blinkState;
    if (blinkState) strip.setPixelColor(0, strip.Color(255,255,0));
    else           strip.clear();
    strip.show();
  }
  else if (gpredictConnected) {
    strip.setPixelColor(0, strip.Color(0,100,0));
    strip.show();
  }

  display.clearDisplay();
  display.setCursor(0,0);   display.print("AZ:"); display.print(az,1);
  display.setCursor(0,8);   display.print("EL:"); display.print(el,1);
  display.setCursor(70,0);  display.print("S:");  display.print(sys);
  display.setCursor(70,8);  display.print("G:");  display.print(gyro);
  display.setCursor(70,16); display.print("A:");  display.print(accel);
  display.setCursor(70,24); display.print("M:");  display.print(mag);
  if (gpredictConnected) {
    display.setCursor(0,56);
    display.print("Connected");
  }
  display.display();

  updateMovement();
}

void handleCommand(const String &cmd) {
  if (!gpredictConnected) {
    gpredictConnected = true;
    strip.setPixelColor(0, strip.Color(0,100,0));
    strip.show();
  }
  if (cmd.startsWith("AZ")) {
    if (cmd.length()>3 && isDigit(cmd.charAt(3))) {
      targetAz = cmd.substring(3).toFloat();
      azMove = true;
      Serial.print("RPRT 0\r");
    } else {
      sensors_event_t ev; bno.getEvent(&ev);
      char buf[16];
      snprintf(buf, sizeof(buf), "AZ %.1f", readHeading(ev));
      Serial.print(buf); Serial.print("\r");
    }
  }
  else if (cmd.startsWith("EL")) {
    if (cmd.length()>3 && isDigit(cmd.charAt(3))) {
      targetEl = cmd.substring(3).toFloat();
      elMove = true;
      Serial.print("RPRT 0\r");
    } else {
      sensors_event_t ev; bno.getEvent(&ev);
      float raw = ev.orientation.y + 47.0f; pushEl(raw);
      char buf[16];
      snprintf(buf, sizeof(buf), "EL %.1f", getSmoothedEl());
      Serial.print(buf); Serial.print("\r");
    }
  }
  else if (cmd.equalsIgnoreCase("P")) {
    sensors_event_t ev; bno.getEvent(&ev);
    float heading = readHeading(ev);
    float raw = ev.orientation.y + 47.0f; pushEl(raw);
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f %.1f", heading, getSmoothedEl());
    Serial.print(buf); Serial.print("\r");
  }
  else if (cmd.equalsIgnoreCase("CALSTATS")) {
    printCalStats();
    Serial.print("RPRT 0\r");
  }
  else if (cmd.equalsIgnoreCase("CALIBRATE")) {
    Serial.println("Re-running calibration...");
    calibrateAndStore();
    Serial.print("RPRT 0\r");
  }
  else if (cmd.equalsIgnoreCase("SA")) {
    digitalWrite(relayAzLeft, RELAY_INACTIVE_LEVEL);
    digitalWrite(relayAzRight, RELAY_INACTIVE_LEVEL);
    Serial.print("RPRT 0\r");
  }
  else if (cmd.equalsIgnoreCase("SE")) {
    digitalWrite(relayElUp, RELAY_INACTIVE_LEVEL);
    digitalWrite(relayElDown, RELAY_INACTIVE_LEVEL);
    Serial.print("RPRT 0\r");
  }
  else if (cmd.equalsIgnoreCase("VE")) {
    Serial.print("VEAdamESP32v1.0\r");
  }
  else if (cmd.equalsIgnoreCase("Q")) {
    gpredictConnected = false;
    strip.clear(); strip.show();
  }
  else {
    Serial.print("RPRT -1\r");
  }
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c=='\r' || c=='\n') {
      if (inputCommand.length()) {
        inputCommand.trim();
        handleCommand(inputCommand);
        inputCommand = "";
      }
    } else inputCommand += c;
  }

  if (millis() - lastUpdateMs >= updateInterval) {
    updateOrientationDisplay();
    lastUpdateMs = millis();
  }
}
