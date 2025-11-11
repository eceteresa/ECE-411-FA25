#include <SPI.h>
#include <MFRC522.h>
#include <MFRC522DriverSPI.h>
#include <MFRC522Debug.h>
#include <MFRC522DriverPinSimple.h>

#define SS_PIN 10
#define LOCK_PIN 4 // Pin for the lock/LED, Using NeoPixel LED

MFRC522 mfrc522(SS_PIN);

// Store the UID of the tag to be used
byte masterTagUID[] = {0xB1, 0x3F, 0x6B, 0xA2}; // Replace with your tag's UID

bool isLocked = true; // Initial state: Locked
unsigned long lastTapTime = 0;
int tapCount = 0;
const unsigned long tapInterval = 2000; // 1 second interval for multiple taps
const int tapsRequired = 2; // Number of taps to toggle lock state

void setup() {
    Serial.begin(115200);
    SPI.begin();
    mfrc522.PCD_Init();
    pinMode(LOCK_PIN, OUTPUT);
    digitalWrite(LOCK_PIN, HIGH); // Lock initially (assuming HIGH locks)
    Serial.println("Place your card on reader...");
}

void loop() {
    // Check for new cards
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
        
        // Check if the detected tag is the master tag
        if (compareUID(mfrc522.uid.uidByte, masterTagUID)) {
            unsigned long currentTime = millis();

            // Check if taps are within the interval
            if (currentTime - lastTapTime < tapInterval) {
                tapCount++;
            } else {
                tapCount = 1; // Reset count if too much time passed
            }

            lastTapTime = currentTime;

            // Check if required taps reached
            if (tapCount == tapsRequired) {
                tapCount = 0; // Reset count
                toggleLockState();
            }
        }

        // Halt PICC (tag)
        mfrc522.PICC_HaltA();
        // Stop encryption on PCD (reader)
        mfrc522.PCD_StopCrypto1();
    }
}

void toggleLockState() {
    isLocked = !isLocked; // Toggle the state
    if (isLocked) {
        digitalWrite(LOCK_PIN, HIGH); // Lock
        Serial.println("System Locked");
    } else {
        digitalWrite(LOCK_PIN, LOW); // Unlock
        Serial.println("System Unlocked for 5 seconds");
        // Optional: automatically re-lock after a delay
        delay(5000); 
        isLocked = true;
        digitalWrite(LOCK_PIN, HIGH);
        Serial.println("System Re-locked automatically");
    }
}

// Helper function to compare two UIDs
bool compareUID(byte* uid1, byte* uid2) {
    for (byte i = 0; i < mfrc522.uid.size; i++) {
        if (uid1[i] != uid2[i]) {
            return false;
        }
    }
    return true;
}
