#include <Adafruit_NeoPixel.h> // Include the Adafruit library

#define PIN_FRONT GPIO5      // headlight pin
#define PIN_BACK GPIO6      // taillight pin
#define NUM_LEDS 5 // Number of LEDs on the strip

Adafruit_NeoPixel front_strip = Adafruit_NeoPixel(NUM_LEDS, PIN_FRONT, NEO_GRB + NEO_KHZ800); //headlights
Adafruit_NeoPixel back_strip = Adafruit_NeoPixel(NUM_LEDS, PIN_BACK, NEO_GRB + NEO_KHZ800); //taillights

void setup() {
  front_strip.begin(); // Initialize the strip
  back_strip.begin(); //initialize the back strip
  front_strip.show(); // Display the color on the strip
  back_strip.show(); //display color on strip
}

void loop() {
  //CALL FUNCTIONS HERE
  delay(1000);   // Wait for 1 second

}

void headlights() {
  // Turn on LED headlight strips
  for(int i=0; i<NUM_LEDS; i++) {
    front_strip.setPixelColor(i, front_strip.Color(255, 255, 255)); // white for headlights
  }

  front_strip.show(); // Update the strip to show the color
}

void tailightsNorm(){
  for(int i=0; i<NUM_LEDS; i++) {
    back_strip.setPixelColor(i, back_strip.Color(150, 0, 0)); // red for taillights
  }
  back_strip.show(); //update the tail light to show color
}

void taillightsBraking(){
    for(int i=0; i<NUM_LEDS; i++) {
    back_strip.setPixelColor(i, back_strip.Color(255, 0, 0)); // red for taillights
  }
  back_strip.show(); //update the tail light to show color
}
