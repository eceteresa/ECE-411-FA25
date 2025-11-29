#include <Adafruit_NeoPixel.h> // Include the Adafruit library

#define PIN_FRONT A1      // Pin connected to the NeoPixel strip
#define PIN_BACK A2
#define NUM_LEDS 5 // Number of LEDs on the strip

Adafruit_NeoPixel front_strip = Adafruit_NeoPixel(NUM_LEDS, PIN_FRONT, NEO_GRB + NEO_KHZ800); //headlights
Adafruit_NeoPixel back_strip = Adafruit_NeoPixel(NUM_LEDS, PIN_BACK, NEO_GRB + NEO_KHZ800); //taillights

void setup() {
  front_strip.begin(); // Initialize the strip
  front_strip.show(); // Display the color on the strip
}

void loop() {
  // --- Solid Color Example (Green) ---
  // Set all pixels to a specific color (R, G, B values from 0-255)
  for(int i=0; i<NUM_LEDS; i++) {
    front_strip.setPixelColor(i, front_strip.Color(150, 0, 0)); // red
  }
  front_strip.show(); // Update the strip to show the color
  delay(1000);   // Wait for 1 second

  // --- Rainbow Cycle Example ---
  rainbowCycle(20); // Run a rainbow cycle effect with a 20ms delay per step
}

// Function to create a rainbow effect
void rainbowCycle(uint8_t wait) {
  for(uint16_t j=0; j < 256*5; j++) { // 5 cycles of all colors on wheel
    for(uint16_t i=0; i< back_strip.numPixels(); i++) {
      back_strip.setPixelColor(i, Wheel(((i * 256 / back_strip.numPixels()) + j) & 255));
    }
    back_strip.show();
    delay(wait);
  }
}

// Input a value 0 to 255 to get a color value.
// The colors are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return back_strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return back_strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return back_strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}
