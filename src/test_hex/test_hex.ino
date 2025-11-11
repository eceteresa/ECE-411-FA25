/*
test printing hex values from RFID tag data

TEST DATA: 0xB1, 0x3F, 0x6B, 0xA2
*/

#include <Arduino.h>

void setup() {
  Serial.begin(9600); // Initialize Serial communication
  delay(1000);        // Give time for Serial Monitor to open

  Serial.println("Hex Print Test:");

  // Test data
  byte testData[] = {0xB1, 0x3F, 0x6B, 0xA2};
  size_t dataSize = sizeof(testData) / sizeof(testData[0]);

  Serial.print("Data in hex: ");
  for (size_t i = 0; i < dataSize; i++) {
    if (testData[i] < 0x10) {
      Serial.print("0"); // Leading zero for single digit hex values
    }
    Serial.print(testData[i], HEX);
    if (i < dataSize - 1) {
      Serial.print(" "); // Space between bytes
    }
  }
  Serial.println();
}

void loop() {
  // nothing to loop — run once
}
