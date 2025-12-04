#include <SPI.h>
#include <MFRC522v2.h>
#include <MFRC522DriverSPI.h>
#include <MFRC522DriverPinSimple.h>
#include <Adafruit_NeoPixel.h>

#define RIFD_SS_PIN  10
#define LOCK_PIN     4
#define NEOPIXEL_PIN 5
#define NUMPIXELS    5

Adafruit_NeoPixel pixels(NUMPIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// RFID driver (driver-based API)
MFRC522DriverPinSimple csPin(RFID_SS_PIN);
MFRC522DriverSPI driver(csPin, SPI, SPISettings(1000000, MSBFIRST, SPI_MODE0));
MFRC522 mfrc522(driver);

// Hard-coded UID of allowed badge (replace with your UID)
byte allowedUID[] = {0xB1, 0x3F, 0x6B, 0xA2};
const byte allowedUIDLength = 4;

// Modes:
// 0 = idle/locked
// 1 = first tap seen (green on LED0), waiting for second tap (3s window)
// 2 = DISARMED (purple persistent), waiting for optional arm (3s arm window)
// 3 = ARMED (red persistent)
byte mode = 0;

// timing variables
unsigned long lastTapTime = 0;        // when the first tap happened (mode 1)
unsigned long armWindowStart = 0;     // when DISARMED window started (mode 2)
const unsigned long doubleTapWindow = 3000UL; // 3 seconds

// Flash/blink helper for the "arm window expired" purple blink
bool flashActive = false;
unsigned long flashStart = 0;
const unsigned long flashDuration = 1000UL; // 1 second, UL = Unsigned Long

// Helper: set LEDs for a mode
void setModeLEDs(byte m) {
  pixels.clear();
  switch (m) {
    case 0: // idle => all off
      break;
    case 1: // waiting for second tap => LED0 green
      pixels.setPixelColor(0, pixels.Color(0, 150, 0));
      break;
    case 2: // DISARMED => all purple
      for (int i = 0; i < NUMPIXELS; i++) pixels.setPixelColor(i, pixels.Color(150, 0, 150));
      break;
    case 3: // ARMED => all red
      for (int i = 0; i < NUMPIXELS; i++) pixels.setPixelColor(i, pixels.Color(150, 0, 0));
      break;
  }
  pixels.show();
}

// Simple UID comparison
bool compareUID(const byte *a, const byte *b, byte length) {
  for (byte i = 0; i < length; i++) {
    if (a[i] != b[i]) return false;
  }
  return true;
}

void lockInit() {
  pinMode(LOCK_PIN, OUTPUT);
  digitalWrite(LOCK_PIN, HIGH); // locked initially (if used)
}

void setup() {
  Serial.begin(115200);
  SPI.begin();

  lockInit();

  Serial.println("Initializing MFRC522 (driver-based)...");
  mfrc522.PCD_Init();

  // NeoPixel init
  pixels.begin();
  pixels.setBrightness(50);
  pixels.clear();
  pixels.show();

  Serial.println("Ready - scan a card.");
}

void loop() {
  unsigned long now = millis();

  // ---------- handle expiration of first-tap 3s window ----------
  if (mode == 1) {
    if ((now - lastTapTime) >= doubleTapWindow) {
      // window expired — reset to idle
      Serial.println("First-tap window expired -> reset to idle.");
      mode = 0;
      setModeLEDs(mode);
      lastTapTime = 0;
    }
  }

  // ---------- handle expiration of arm window (mode 2) ----------
  if (mode == 2 && armWindowStart != 0) {
    if ((now - armWindowStart) >= doubleTapWindow) {
      // arm window expired: stay DISARMED, but blink purple for 1s
      Serial.println("Arm window expired while DISARMED -> blinking purple for 1s.");
      armWindowStart = 0; // clear the arm window
      // trigger a 1s blink: we'll turn LEDs off briefly then show purple for flashDuration
      pixels.clear();
      pixels.show();
      flashActive = true;
      flashStart = now; // flash will show purple for flashDuration then restore persistent purple
      // note: we do NOT change mode (remain mode==2)
    }
  }

  // ---------- handle flash timer ----------
  if (flashActive) {
    if ((now - flashStart) < 50) {
      // small off period already shown (we cleared above), do nothing
    } else if ((now - flashStart) < flashDuration) {
      // show purple flash during this window
      for (int i = 0; i < NUMPIXELS; i++) pixels.setPixelColor(i, pixels.Color(150, 0, 150));
      pixels.show();
    } else {
      // flash finished — restore persistent DISARMED purple
      flashActive = false;
      setModeLEDs(2);
    }
  }

  // ---------- RFID read ----------
  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial()) return;

  // print uid
  Serial.print("Scanned UID: ");
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) Serial.print('0');
    Serial.print(mfrc522.uid.uidByte[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  // check allowed badge
  if (mfrc522.uid.size == allowedUIDLength &&
      compareUID(mfrc522.uid.uidByte, allowedUID, allowedUIDLength)) {

    Serial.println("Authorized badge detected!");

    // Behavior depends on current mode:
    if (mode == 0) {
      // First tap: light LED0 green and start 3s window
      Serial.println("Mode 0 -> First tap: LED0 GREEN, start 3s window (mode=1).");
      mode = 1;
      lastTapTime = now;
      setModeLEDs(mode); // LED0 green
    }
    else if (mode == 1) {
      // Second tap within first 3s => enter DISARMED
      if ((now - lastTapTime) <= doubleTapWindow) {
        Serial.println("Mode 1 -> Second tap within window: DISARMED (mode=2).");
        mode = 2;
        armWindowStart = now; // start arm window (3s) to allow arming
        setModeLEDs(mode);    // all purple persistent
      } else {
        // If too late, treat as a fresh first tap
        Serial.println("Mode 1 -> Second tap TOO LATE: treat as first tap again.");
        mode = 1;
        lastTapTime = now;
        setModeLEDs(mode);
      }
    }
    else if (mode == 2) {
      // We're DISARMED. If an armWindow is active and this tap is within it, ARM the system.
      if (armWindowStart != 0 && (now - armWindowStart) <= doubleTapWindow) {
        Serial.println("Mode 2 -> Third tap within arm window: ARMED (mode=3).");
        mode = 3;
        armWindowStart = 0;
        setModeLEDs(mode); // all red persistent
      } else {
        // If no arm window active (or expired), treat this as starting the arm window again:
        Serial.println("Mode 2 -> Tap received: start arm window (3s) to allow arming.");
        armWindowStart = now;
        // Keep pixels purple (already set)
      }
    }
    else if (mode == 3) {
      // Already ARMED. You could define behavior — here we'll treat as "start sequence over"
      Serial.println("Mode 3 (ARMED) tap: resetting to waiting-for-2nd-tap (mode=1).");
      mode = 1;
      lastTapTime = now;
      setModeLEDs(mode);
    }
  }
  else {
    Serial.println("Access denied - unknown badge.");
    // Denied feedback: red on LED0 for 1s (blocking). If you want non-blocking, we can change this.
    pixels.clear();
    pixels.setPixelColor(0, pixels.Color(255, 0, 0));
    pixels.show();
    delay(1000);
    pixels.clear();
    pixels.show();
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}

