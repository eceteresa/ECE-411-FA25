#include <SPI.h>
#include <Adafruit_LIS3DH.h>
#include <Adafruit_Sensor.h>
#include <MFRC522v2.h>
#include <MFRC522DriverSPI.h>
#include <MFRC522DriverPinSimple.h>
#include <Adafruit_NeoPixel.h>

// --- Pin assignments ---
#define LIS3DH_CS 9
#define RFID_CS   10
#define RFID_RST  22
#define LED_FRONT 5   // front NeoPixel data pin
//#define LED_REAR  11  // breadboard rear LED (future use)
#define LED_REAR 6 //Protoboard GPIOpin
#define FRONT_LEDS 5
#define REAR_LEDS 5
#define LOCK_PIN  4   // lock / test pin
#define BUTTZER   11  // buzzer pin

// --- SPI bus pins (ESP32-S3 remap) ---
#define SPI_SCK   36
#define SPI_MISO  37
#define SPI_MOSI  35

//Accelerometer logic
const float FORWARD_THRESH     = 0.5;        // m/s^2: > this = "moving forward"
const float BRAKE_THRESH       = -0.5;       // m/s^2: < this = "braking / slowing"
const unsigned long REAR_BLINK_INTERVAL = 1000;   // ms
const unsigned long BRAKE_HOLD_MS       = 2000;  // ms

// --- ARMED shake / alarm tuning ---
const float CHAOS_DELTA_THRESH            = 3.0;       // m/s^2 per axis → "chaotic shake"
const unsigned long CHAOS_TIME_MS   = 3000;     // 5 s continuous chaos to trigger alarm
const unsigned long ALARM_DURATION_MS = 30000;   // 30 s alarm duration
const unsigned long BUZZ_TOGGLE_MS  = 300;       // beep on/off toggle during alarm

bool inChaos      = false;        // ARMED: currently seeing chaotic motion
unsigned long chaosStart = 0;     // when chaotic motion started

bool alarmActive  = false;        // ARMED: alarm is currently sounding
unsigned long alarmEnd   = 0;     // millis when alarm should stop

bool buzzerOn     = false;        // for alarm toggling
unsigned long lastBuzzToggle = 0; // last toggle time


// DISARMED brightness levels
const uint8_t RIDE_BRIGHTNESS = 200;   // while biking (forward accel)
const uint8_t BRAKE_BRIGHTNESS = 255;  // brake / stopped, full brightness


// --- Devices ---
Adafruit_LIS3DH lis = Adafruit_LIS3DH(LIS3DH_CS);

// NeoPixel strip: all 10 pixels driven from LED_FRONT
Adafruit_NeoPixel frontStrip(FRONT_LEDS, LED_FRONT, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel rearStrip(REAR_LEDS, LED_REAR, NEO_GRB + NEO_KHZ800);

// MFRC522 RFID reader
MFRC522DriverPinSimple ss_pin(RFID_CS);
MFRC522DriverSPI       driver(ss_pin);
MFRC522                mfrc522(driver);

// Known UID (master card)
byte masterTagUID[] = {0xB1, 0x3F, 0x6B, 0xA2};

// Boolean logic
bool direction;      // accelerometer logic (future)
bool isLocked = true;
bool armed    = false;  // global ARMED state
bool lightsEnabled = false; // bike-light logic active only in DISARMED

// Baseline acceleration for ARMED mode (accounting for gravity)
float baseAx = 0.0;
float baseAy = 0.0;
float baseAz = 0.0;
bool  baseAccelValid = false;

// ---------- Helpers ----------

// Compare UID helper
bool compareUID(byte* uid1, byte* uid2, byte size) {
  for (byte i = 0; i < size; i++) {
    if (uid1[i] != uid2[i]) return false;
  }
  return true;
}

// Buzzer: wrong UID
void beepError() {
  tone(BUTTZER, 1000, 500);  // 2 kHz for 500 ms
  delay(500);
  noTone(BUTTZER);
}

// Single status LED: front strip, pixel 0
void setStatusLED(uint8_t r, uint8_t g, uint8_t b) {
  frontStrip.setPixelColor(0, frontStrip.Color(r, g, b));
  frontStrip.show();
}

// All front pixels
void setFrontStripColor(uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i < FRONT_LEDS; i++) {
    frontStrip.setPixelColor(i, frontStrip.Color(r, g, b));
  }
  frontStrip.show();
}

// All rear pixels
void setRearStripColor(uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i < REAR_LEDS; i++) {
    rearStrip.setPixelColor(i, rearStrip.Color(r, g, b));
  }
  rearStrip.show();
}

void clearAllLights() {
  // Turn off all front + rear pixels
  setFrontStripColor(0, 0, 0);
  setRearStripColor(0, 0, 0);
}


// 4s timer for 1 tap (DISARMED) or 2+ taps (ARMED)
void selectArmingMode() {
  uint8_t tapCount = 0;
  unsigned long start  = millis();
  const unsigned long window = 4000; // 4 seconds

  Serial.println("ARMED/DISARMED window open (4s). Tap master card now.");
  Serial.println("  - 1 tap = DISARMED (purple front strip)");
  Serial.println("  - 2+ taps = ARMED (red front strip)");

  while (millis() - start < window) {
    // Look for a new card
    if (!mfrc522.PICC_IsNewCardPresent()) {
      continue;  // no new card, keep waiting
    }
    if (!mfrc522.PICC_ReadCardSerial()) {
      continue;  // saw something but couldn't read
    }

    byte uidSize = mfrc522.uid.size;
    byte* uid    = mfrc522.uid.uidByte;

    // Is it the master tag?
    if (compareUID(uid, masterTagUID, uidSize)) {
      tapCount++;
      Serial.print("Master card tap #");
      Serial.println(tapCount);

      // simple debounce so one long hold doesn't count as multiple taps
      delay(300);

      //TODO: Decide if we want to keep this or not
      // Optional quick visual feedback on each tap
      setFrontStripColor(0, 0, 50);   // faint blue blink
      delay(150);
      setFrontStripColor(0, 0, 0);

      // Halt card
      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();
    } else {
      // ignore strangers during arming window
      Serial.println("Non-master badge during arming window (ignored).");
      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();
    }
  }

  // Decide mode based on tapCount
  if (tapCount == 0) {
    Serial.println("No taps detected. Keeping previous ARMED/DISARMED state.");
    return;  // leave 'armed' and LED colors as-is
  } else if (tapCount == 1) {
    armed = false;  // DISARMED
    lightsEnabled = true;
    Serial.println("DISARMED selected (1 tap). Front LEDs = purple.");
   
    //DISARMED purple LEDs. Stays lit for 1s
    setFrontStripColor(128, 0, 128);  // purple 
    delay(1000);
    setFrontStripColor(0, 0, 0);
    setRearStripColor(0, 0, 0);
  } else { // 2 or more taps => ARMED with all red LEDs
    armed = true;
    lightsEnabled = false;
    Serial.println("ARMED selected (2+ taps). Disabling bike-light motion logic for now.");
    
    //setting ARMED state LED colors
    setFrontStripColor(255, 0, 0);    // red
    delay(1000);
    setFrontStripColor(0, 0, 0); //Turn off LEDS

    // Take baseline acceleration (including gravity) at the moment of arming
  sensors_event_t event;
  lis.getEvent(&event);
  baseAx = event.acceleration.x;
  baseAy = event.acceleration.y;
  baseAz = event.acceleration.z;
  baseAccelValid = true;

  Serial.print("ARMED baseline set: ax=");
  Serial.print(baseAx);
  Serial.print(" ay=");
  Serial.print(baseAy);
  Serial.print(" az=");
  Serial.println(baseAz);
  }
}

// Scale RGB by brightness (0–255)
uint32_t scaleColor(Adafruit_NeoPixel &strip, uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
  uint16_t scaledR = (r * brightness) / 255;
  uint16_t scaledG = (g * brightness) / 255;
  uint16_t scaledB = (b * brightness) / 255;
  return strip.Color(scaledR, scaledG, scaledB);
}

void setFrontScaled(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
  for (int i = 0; i < FRONT_LEDS; i++) {
    frontStrip.setPixelColor(i, scaleColor(frontStrip, r, g, b, brightness));
  }
  frontStrip.show();
}

void setRearScaled(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
  for (int i = 0; i < REAR_LEDS; i++) {
    rearStrip.setPixelColor(i, scaleColor(rearStrip, r, g, b, brightness));
  }
  rearStrip.show();
}


// ---------- Motion-based light logic ----------
void updateMotionLights() {
  // Only run bike-light motion logic when in DISARMED mode
  if (!lightsEnabled || armed) {
    return;  // do nothing; whatever static colors we set stay as-is
  }

  static unsigned long lastBlink = 0;
  static bool rearOn = false;
  static unsigned long brakeUntil = 0;

  sensors_event_t event;
  lis.getEvent(&event);

  float ax = event.acceleration.x;  // m/s^2 along X
  unsigned long now = millis();

  // FORWARD / ACCELERATING
  if (ax > FORWARD_THRESH) {
    // Cancel any brake hold
    brakeUntil = 0;

    // Front: solid white
      setFrontScaled(255, 255, 255, RIDE_BRIGHTNESS);

    // REAR: blinking red at 200 brightness
    if (now - lastBlink >= REAR_BLINK_INTERVAL) {
      rearOn = !rearOn;
      if (rearOn) setRearScaled(255, 0, 0, RIDE_BRIGHTNESS);
      else        setRearScaled(0,   0, 0, 0);
      lastBlink = now;
    }

    return;

  }

  // BRAKING / SLOWING DOWN (negative accel)
  if (ax < BRAKE_THRESH) {
    // If this is the first time we notice a brake, start the hold window
    if (brakeUntil == 0) {
      brakeUntil = now + BRAKE_HOLD_MS;
      Serial.println("[DISARMED] Brake detected, switching to full brightness.");
    }

    // FRONT: white at full 255
    setFrontScaled(255, 255, 255, BRAKE_BRIGHTNESS);

    // REAR: solid red at full 255
    setRearScaled(255, 0, 0, BRAKE_BRIGHTNESS);

    return;
  }

// --- IDLE / COASTING ---
  // treat this like braking (bike is stopped = brake light)
  setFrontScaled(255, 255, 255, BRAKE_BRIGHTNESS);
  setRearScaled(255, 0, 0, BRAKE_BRIGHTNESS);
}

// ---------- ARMED shake-detection + alarm logic ----------
void updateArmedAlarm() {
  // Only care when ARMED
  if (!armed) {
    return;
  }

  unsigned long now = millis();

  // If alarm is currently active, keep running it for ALARM_DURATION_MS
  if (alarmActive) {
    if (now >= alarmEnd) {
      // Stop alarm
      Serial.println("[ARMED] Alarm duration ended, stopping buzzer.");
      alarmActive = false;
      noTone(BUTTZER);
      buzzerOn = false;
      return;
    }

    // Run repeating buzzer pattern (non-blocking)
    if (now - lastBuzzToggle >= BUZZ_TOGGLE_MS) {
      buzzerOn = !buzzerOn;
      if (buzzerOn) {
        tone(BUTTZER, 2500);  // alarm tone
      } else {
        noTone(BUTTZER);
      }
      lastBuzzToggle = now;
    }

    return;  // don't check new motion while screaming
  }

  // If we reach here, ARMED but no active alarm yet.
  // Check accelerometer to see if we are in "chaotic" motion (change from baseline).
  if (!baseAccelValid) {
    // Shouldn't happen, but just in case:
    Serial.println("[ARMED] Warning: baseline accel not valid; skipping chaos check.");
    return;
  }

  sensors_event_t event;
  lis.getEvent(&event);
  float ax = event.acceleration.x;
  float ay = event.acceleration.y;
  float az = event.acceleration.z;

  // Delta from baseline (removes static gravity at arming orientation)
  float dax = ax - baseAx;
  float day = ay - baseAy;
  float daz = az - baseAz;

  float dMag = sqrtf(dax*dax + day*day + daz*daz);
  Serial.print("[ARMED] dMag=");
  Serial.print(dMag, 2);
  Serial.print(" (threshold ");
  Serial.print(CHAOS_DELTA_THRESH, 2);
  Serial.println(")");

  bool chaotic = (dMag > CHAOS_DELTA_THRESH);

  if (chaotic) {
    if (!inChaos) {
      // Just entered chaotic region
      inChaos = true;
      chaosStart = now;
      Serial.println("[ARMED] Chaotic movement detected! Starting 10s chaos timer...");
    } else {
      // Already in chaos; check if time exceeded threshold
      unsigned long elapsed = now - chaosStart;
      if (elapsed >= CHAOS_TIME_MS) {
        Serial.println("[ARMED] Chaotic movement sustained for 10s. TRIGGERING ALARM!");
        alarmActive = true;
        alarmEnd = now + ALARM_DURATION_MS;

        // Start buzzer immediately
        buzzerOn = true;
        tone(BUTTZER, 2500);
        lastBuzzToggle = now;
      } else {
        Serial.print("[ARMED] Chaotic movement ongoing: ");
        Serial.print(elapsed / 1000.0, 1);
        Serial.println(" s (need 10 s to alarm).");
      }
    }
  } else {
    // Not chaotic right now
    if (inChaos) {
      // We were in chaos but it stopped before timer expired
      unsigned long elapsed = now - chaosStart;
      Serial.print("[ARMED] Chaotic movement STOPPED after ");
      Serial.print(elapsed / 1000.0, 1);
      Serial.println(" s (<10 s) → treating as harmless jostle; resetting timer.");
      inChaos = false;
      chaosStart = 0;
    }
  }
}




// ---------- SETUP ----------

void setup() {
  Serial.begin(115200);
  // while (!Serial);  // optional; can comment out if it hangs

  Serial.println("\nInitializing SPI and devices...");

  // SPI on custom pins
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);

  // NeoPixel setup
  frontStrip.begin();
  rearStrip.begin();
  frontStrip.setBrightness(80); // 0–255
  rearStrip.setBrightness(80);
  frontStrip.show();            // all off
  rearStrip.show();             //all off

  // Lock + buzzer pins
  pinMode(LOCK_PIN, OUTPUT);
  digitalWrite(LOCK_PIN, HIGH); // locked initially
  pinMode(BUTTZER, OUTPUT);

  // LIS3DH
  if (!lis.begin(LIS3DH_CS)) {
    Serial.println("Could not find LIS3DH. Check wiring!");
    while (1) {
      delay(10);
    }
  }
  lis.setRange(LIS3DH_RANGE_2_G);
  Serial.println("LIS3DH initialized.");

  // RC522
  mfrc522.PCD_Init();
  Serial.println("RC522 initialized.");
  Serial.println("Ready! -- Scan card, plz.");

  // Optional: little startup blink + beep
  setStatusLED(0, 0, 50);
  beepError();
  setStatusLED(0, 0, 0);
}

// ---------- LOOP ----------

void loop() {
  updateMotionLights(); //DISARMED state
  updateArmedAlarm(); // ARMED state

  // Wait for a card
  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial())   return;

  byte uidSize = mfrc522.uid.size;
  byte* uid    = mfrc522.uid.uidByte;

  Serial.print("Scanned UID: ");
  for (byte i = 0; i < uidSize; i++) {
    if (uid[i] < 0x10) Serial.print("0");
    Serial.print(uid[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  if (compareUID(uid, masterTagUID, uidSize)) {
  // Correct badge: always reset visual state first
  Serial.println("Your card is detected");

    if (alarmActive) {
    Serial.println("[RFID] Clearing active alarm and silencing buzzer.");
    alarmActive = false;
    inChaos = false;
    chaosStart = 0;
    noTone(BUTTZER);
    buzzerOn = false;
  }

  // Always reset visual state first
  Serial.println("Resetting LED state and unlocking...");

  clearAllLights();

  // Show "OK" status on front LED 0 (green)
  setStatusLED(0, 255, 0);  // first pixel green
  delay(500);
  setStatusLED(0, 0, 0);    // turn that pixel off again

  // Lock / unlock logic
  digitalWrite(LOCK_PIN, LOW);   // unlock
  delay(3000);
  digitalWrite(LOCK_PIN, HIGH);  // lock
  Serial.println("Locked again.");

  // Halt current card before arming mode
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

  // 4) 4s arming/disarming selection window
  selectArmingMode();
}


  else {
    // Wrong badge
    Serial.println("Who is this? Denied!! Stranger Danger!");
    setStatusLED(255, 0, 0);  // first pixel red
    delay(500);
    setStatusLED(0, 0, 0);
    beepError();

    // Halt this card too
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
  }

  // After this, loop() returns and waits for next card
}
