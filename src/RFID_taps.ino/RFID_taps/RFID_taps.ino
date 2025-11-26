#include <SPI.h>
#include <MFRC522v2.h>
#include <MFRC522DriverSPI.h>
#include <MFRC522DriverPinSimple.h>

#define SS_PIN 10
#define LOCK_PIN 4

// Use the correct CS pin class for your library:
MFRC522DriverPinSimple csPin(SS_PIN);

// Create SPI driver with required SPISettings:
MFRC522DriverSPI driver(csPin, SPI, SPISettings(1000000, MSBFIRST, SPI_MODE0));

// Create MFRC522 object using the driver:
MFRC522 mfrc522(driver);

// Known UID (replace with yours)
byte masterTagUID[] = {0xB1, 0x3F, 0x6B, 0xA2};

bool isLocked = true;


// Compare UID helper
bool compareUID(byte* uid1, byte* uid2, byte size) {
    for (byte i = 0; i < size; i++) {
        if (uid1[i] != uid2[i]) return false;
    }
    return true;
}


void setup() {
    Serial.begin(115200);
    SPI.begin();

    pinMode(LOCK_PIN, OUTPUT);
    digitalWrite(LOCK_PIN, HIGH); // locked initially

    Serial.println("Initializing MFRC522...");
    mfrc522.PCD_Init();

    Serial.println("Ready – scan a card.");
}


void loop() {

    if (!mfrc522.PICC_IsNewCardPresent()) return;
    if (!mfrc522.PICC_ReadCardSerial()) return;

    byte uidSize = mfrc522.uid.size;
    byte* uid = mfrc522.uid.uidByte;

    Serial.print("Scanned UID: ");
    for (byte i = 0; i < uidSize; i++) {
        if (uid[i] < 0x10) Serial.print("0");
        Serial.print(uid[i], HEX);
        Serial.print(" ");
    }
    Serial.println();

    if (compareUID(uid, masterTagUID, uidSize)) {
        Serial.println("Your card is detected – unlocking...");
        digitalWrite(LOCK_PIN, LOW);
        delay(3000);
        digitalWrite(LOCK_PIN, HIGH);
        Serial.println("Locked again.");
    } else {
        Serial.println("Who is this? Denied!! Stranger Danger!!!.");
    }

    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
}
