#include <SPI.h>
#include <Adafruit_LIS3DH.h>
#include <Adafruit_Sensor.h>
#include <MFRC522.h>

// --- Pin assignments ---
#define LIS3DH_CS 9
#define RFID_CS   10
#define RFID_RST  22

// --- SPI bus pins ---
#define SPI_SCK   36
#define SPI_MISO  37
#define SPI_MOSI  35

// --- Devices ---
Adafruit_LIS3DH lis = Adafruit_LIS3DH(LIS3DH_CS);
MFRC522 mfrc522(RFID_CS, RFID_RST);

void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.println("\nInitializing SPI devices...");
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);  // custom SPI pins

  // --- LIS3DH ---
  if (!lis.begin(LIS3DH_CS)) {
    Serial.println("Could not find LIS3DH. Check wiring!");
    while (1);
  }
  lis.setRange(LIS3DH_RANGE_2_G);
  Serial.println("LIS3DH initialized.");


  // --- RC522 ---
  mfrc522.PCD_Init();  // initialize RFID
  Serial.println("RC522 initialized.");

}

void loop() {
  // --- Read LIS3DH ---
  sensors_event_t event;
  lis.getEvent(&event);
  Serial.print("Accel (m/s^2): X=");
  Serial.print(event.acceleration.x);
  Serial.print(" Y=");
  Serial.print(event.acceleration.y);
  Serial.print(" Z=");
  Serial.println(event.acceleration.z);

  // --- Check RFID ---
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    Serial.print("RFID UID:");
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
      Serial.print(mfrc522.uid.uidByte[i], HEX);
    }
    Serial.println();
    mfrc522.PICC_HaltA();
  }

  delay(500);
}
