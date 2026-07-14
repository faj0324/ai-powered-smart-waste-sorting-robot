#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// ---------- PINS ----------
const int RED_LED    = 6;
const int YELLOW_LED = 7;
const int GREEN_LED  = 8;

const int MG996R_PIN = 9;    // platform rotation servo (POSITIONAL, not continuous)
const int SG90_PIN   = 10;   // flap gate servo

const int CONFIRM_BTN = 2;   // do not change / remove
const int REJECT_BTN  = 3;   // do not change / remove



// Four HC-SR04 ultrasonic sensors, one per compartment.
// Order matches MATERIALS: METAL, PLASTIC, PAPER, GENERAL.
// !! Set these to your actual wiring. The old single-sensor build used 11/12. !!
const int TRIG_PIN[4] = {22, 24, 26, 28};
const int ECHO_PIN[4] = {23, 25, 27, 29};

Servo mg996r;
Servo sg90;

// ---------- SETTINGS ----------
const int CONF_THRESHOLD = 70;   // >=70 auto-sorts, <70 opens confirm window

// MG996R positional angles. Platform sits at NEUTRAL, rotates to the bin, returns.
// Calibrate BIN_ANGLE against your real platform before the demo.
const int NEUTRAL_ANGLE = 90;
const int BIN_ANGLE[4]  = {0, 60, 120, 180};   // METAL, PLASTIC, PAPER, GENERAL
const int SERVO_STEP_MS = 15;                  // smaller = faster, larger = smoother/slower

const int SG90_CLOSED = 170;
const int SG90_OPEN   = 0;

// Bin full: distance drops to 3-4 cm and stays there for 5 consecutive reads.
const int BIN_FULL_DISTANCE_CM   = 4;
const int CONSECUTIVE_FULL_READS = 5;

const char* MATERIALS[4] = {"METAL", "PLASTIC", "PAPER", "GENERAL"};

bool busy = false;
unsigned long lastBinCheck = 0;
const unsigned long BIN_CHECK_INTERVAL = 2000;

int fullCounter[4] = {0, 0, 0, 0};   // consecutive near-full reads per bin

// ---------- LED ----------
void allLedsOff() {
  digitalWrite(RED_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
}

// ---------- BUZZER ----------
void beep(int ms) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(ms);
  digitalWrite(BUZZER_PIN, LOW);
}

// ---------- BUTTON ----------
bool pressed(int pin) {
  static unsigned long lastPress[40] = {0};
  if (digitalRead(pin) == LOW && millis() - lastPress[pin] > 250) {
    lastPress[pin] = millis();
    return true;
  }
  return false;
}

// ---------- SERVO ----------
// Move a positional servo smoothly so the platform does not jerk or overshoot.
void moveServoSlowly(Servo &servo, int target, int stepDelay) {
  int cur = servo.read();
  if (cur < target) {
    for (int p = cur; p <= target; p++) { servo.write(p); delay(stepDelay); }
  } else {
    for (int p = cur; p >= target; p--) { servo.write(p); delay(stepDelay); }
  }
}

// ---------- ULTRASONIC ----------
float readDistanceCM(int i) {
  digitalWrite(TRIG_PIN[i], LOW);
  delayMicroseconds(5);
  digitalWrite(TRIG_PIN[i], HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN[i], LOW);
  long duration = pulseIn(ECHO_PIN[i], HIGH, 30000);
  if (duration == 0) {
    return -1;
  }
  return duration * 0.0343 / 2.0;
}

void sendBinStatusToPi() {
  for (int i = 0; i < 4; i++) {
    float distance = readDistanceCM(i);

    // Count consecutive near-full readings. Bin is only reported full after
    // CONSECUTIVE_FULL_READS in a row, to reject single noisy pings.
    if (distance > 0 && distance <= BIN_FULL_DISTANCE_CM) {
      if (fullCounter[i] < CONSECUTIVE_FULL_READS) fullCounter[i]++;
    } else {
      fullCounter[i] = 0;
    }
    int fullStatus = (fullCounter[i] >= CONSECUTIVE_FULL_READS) ? 1 : 0;

    // Python/Blynk receives one line per bin:
    // BIN:index:distance:fullStatus
    // Example: BIN:0:3.5:1  (metal bin, 3.5 cm, full)
    Serial.print("BIN:");
    Serial.print(i);
    Serial.print(":");
    Serial.print(distance, 1);
    Serial.print(":");
    Serial.println(fullStatus);
  }
}

// ---------- IDLE ----------
void goIdle() {
  busy = false;
  allLedsOff();
  digitalWrite(RED_LED, HIGH);
  moveServoSlowly(mg996r, NEUTRAL_ANGLE, SERVO_STEP_MS);
  sg90.write(SG90_CLOSED);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("EcoSort Ready");
  lcd.setCursor(0, 1);
  lcd.print("Waiting camera");
}

// ---------- SORT ----------
void sortItem(int bin) {
  busy = true;
  allLedsOff();
  digitalWrite(YELLOW_LED, HIGH);
  sg90.write(SG90_CLOSED);
  delay(300);

  // Rotate platform to align the correct compartment.
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Moving to:");
  lcd.setCursor(0, 1);
  lcd.print(MATERIALS[bin]);
  moveServoSlowly(mg996r, BIN_ANGLE[bin], SERVO_STEP_MS);
  delay(500);

  // Open the flap gate, let the item drop, close it.
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Dropping:");
  lcd.setCursor(0, 1);
  lcd.print(MATERIALS[bin]);
  sg90.write(SG90_OPEN);
  delay(1200);
  sg90.write(SG90_CLOSED);
  delay(600);

  // Return platform to neutral.
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Returning home");
  moveServoSlowly(mg996r, NEUTRAL_ANGLE, SERVO_STEP_MS);

  allLedsOff();
  digitalWrite(GREEN_LED, HIGH);
  beep(150);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Sorted!");
  lcd.setCursor(0, 1);
  lcd.print(MATERIALS[bin]);
  Serial.print("SORTED:");
  Serial.println(MATERIALS[bin]);
  delay(1500);
  Serial.println("DONE");
  goIdle();
}

// ---------- CONFIRM WINDOW (low confidence / disagreement only) ----------
// Per Section 3.8 this only runs when confidence < 70%.
void waitForUserDecision(int predictedBin, int confidence) {
  unsigned long start = millis();
  unsigned long lastBlink = 0;
  unsigned long lastLcdUpdate = 0;
  bool ledOn = false;

  while (millis() - start < 15000) {
    // Yellow LED blinks while waiting.
    if (millis() - lastBlink > 400) {
      ledOn = !ledOn;
      digitalWrite(YELLOW_LED, ledOn);
      lastBlink = millis();
    }

    // LCD shows the low-confidence class and what the buttons do.
    if (millis() - lastLcdUpdate > 1000) {
      int remaining = 15 - ((millis() - start) / 1000);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("LOW ");
      lcd.print(confidence);
      lcd.print("% ");
      lcd.print(MATERIALS[predictedBin]);
      lcd.setCursor(0, 1);
      lcd.print("C:USE R:GEN ");
      lcd.print(remaining);
      lcd.print("s");
      lastLcdUpdate = millis();
    }

    // CONFIRM = use the predicted bin anyway.
    if (pressed(CONFIRM_BTN)) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("User confirmed");
      lcd.setCursor(0, 1);
      lcd.print(MATERIALS[predictedBin]);
      if (predictedBin == 3) {
        Serial.println("GENERAL_CAPTURE");
      }
      delay(700);
      sortItem(predictedBin);
      return;
    }

    // REJECT = always GENERAL, save the image.
    if (pressed(REJECT_BTN)) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Rejected");
      lcd.setCursor(0, 1);
      lcd.print("To GENERAL");
      Serial.println("GENERAL_CAPTURE");
      delay(700);
      sortItem(3);
      return;
    }
  }

  // Timeout on a low-confidence item goes to GENERAL and saves the image.
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("No confirm");
  lcd.setCursor(0, 1);
  lcd.print("To GENERAL");
  Serial.println("GENERAL_CAPTURE");
  delay(700);
  sortItem(3);
}

// ---------- HANDLE CAMERA RESULT ----------
void handleCameraResult(String line) {
  // Expected from the Pi (fusion already applied on the Pi side):
  // RESULT:PLASTIC:85
  // RESULT:PAPER:62
  // RESULT:NONE:0
  int c1 = line.indexOf(':');
  int c2 = line.indexOf(':', c1 + 1);
  if (c1 < 0 || c2 < 0) {
    busy = false;
    return;
  }

  String material = line.substring(c1 + 1, c2);
  int confidence  = line.substring(c2 + 1).toInt();
  material.toUpperCase();

  int predictedBin = 3; // default GENERAL
  if (material == "METAL")        predictedBin = 0;
  else if (material == "PLASTIC") predictedBin = 1;
  else if (material == "PAPER")   predictedBin = 2;
  else                            predictedBin = 3;

  allLedsOff();
  digitalWrite(YELLOW_LED, HIGH);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(material);
  lcd.print(" ");
  lcd.print(confidence);
  lcd.print("%");
  delay(1000);

  // Section 3.8: >=70% auto-sorts immediately, <70% opens the 15s window.
  if (confidence >= CONF_THRESHOLD) {
    if (predictedBin == 3) {
      Serial.println("GENERAL_CAPTURE");
    }
    sortItem(predictedBin);
  } else {
    waitForUserDecision(predictedBin, confidence);
  }
}

// ---------- SETUP ----------
void setup() {
  Serial.begin(9600);

  pinMode(RED_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(CONFIRM_BTN, INPUT_PULLUP);
  pinMode(REJECT_BTN, INPUT_PULLUP);

  for (int i = 0; i < 4; i++) {
    pinMode(TRIG_PIN[i], OUTPUT);
    pinMode(ECHO_PIN[i], INPUT);
    digitalWrite(TRIG_PIN[i], LOW);
  }

  mg996r.attach(MG996R_PIN);
  sg90.attach(SG90_PIN);

  lcd.init();
  lcd.backlight();
  goIdle();
  Serial.println("ARDUINO_READY");
}

// ---------- LOOP ----------
void loop() {
  // Bin-full monitoring runs in the background, only when not sorting.
  if (!busy && millis() - lastBinCheck >= BIN_CHECK_INTERVAL) {
    lastBinCheck = millis();
    sendBinStatusToPi();
  }

  // Receive fused YOLO result from the Raspberry Pi.
  if (Serial.available() && !busy) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.startsWith("RESULT:")) {
      busy = true;
      handleCameraResult(line);
    }
  }
}
