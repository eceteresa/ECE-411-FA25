#include <SPI.h>
#include <Adafruit_LIS3DH.h>
#include <Adafruit_Sensor.h>
#include <MFRC522.h>
#include <Adafruit_NeoPixel.h> // Include the Adafruit library

// --- Pin assignments ---
#define LIS3DH_CS 9
#define RFID_CS   10
#define RFID_RST  22

// neopixel pin and globals
#define PIN_FRONT A1      // headlight
#define PIN_BACK A2       //tail light
#define NUM_LEDS 5 // Number of LEDs on the strip

// --- SPI bus pins ---
#define SPI_SCK   36
#define SPI_MISO  37
#define SPI_MOSI  35

// --- Devices ---
Adafruit_LIS3DH lis = Adafruit_LIS3DH(LIS3DH_CS);
MFRC522 mfrc522(RFID_CS, RFID_RST);

bool direction;

//neopixel strip info
Adafruit_NeoPixel front_strip = Adafruit_NeoPixel(NUM_LEDS, PIN_FRONT, NEO_GRB + NEO_KHZ800); //headlights
Adafruit_NeoPixel back_strip = Adafruit_NeoPixel(NUM_LEDS, PIN_BACK, NEO_GRB + NEO_KHZ800); //taillights

void setup() {
  // Initializing accelerometer
  Serial.begin(115200);
  while (!Serial);

  Serial.println("\nInitializing SPI devices...");
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);  // custom SPI pins

  // --- Initializing LIS3DH ---
  if (!lis.begin(LIS3DH_CS)) {
    Serial.println("Could not find LIS3DH. Check wiring!");
    while (1);
  }
  lis.setRange(LIS3DH_RANGE_2_G);
  Serial.println("LIS3DH initialized.");

   // --- Initializing RC522 ---
  mfrc522.PCD_Init();  // initialize RFID
  Serial.println("RC522 initialized.");



  // Sensing direction or travel and updating boolean variable
    sensors_event_t event;
  lis.getEvent(&event);
  
  // If accelerometer outputs positive value, direction = forward
  if (event.acceleration.x > 0) {
    Serial.println("accel detected");
    direction = true;
  }
  // If accelerometer reads negative value, bike is slowing down
  else{
    Serial.println("braking detected");
    direction = false;
  }
  
  //neopixel setup
  //may add tail light setup later
  front_strip.begin(); // Initialize the strip
  back_strip.begin();
  front_strip.show(); // Display the color on the strip
  back_strip.show();


} // End setup loop

void loop() {
// Code is split into two WHILE loops
//
// LED LOOP ----------------------------------------------------------
//
// LED loop is entered when pin A3 reads HIGH, meaning the LED switch 
// is in the ON position
//
// LED loop is exited when A3 no longer reads HIGH, meaning the LED
// switch is in the OFF position
//
// -------------------------------------------------------------------
// 
//
// RFID/Alarm Loop ---------------------------------------------------
//
// RFID/Alarm loop is entered when pin A3 reads LOW, meaning the LED 
// switch is in the OFF position
//
// Nested Alarm functionality loop is only entered when RFID is in the 
// ARMED state, otherwise the loop does nothing
//
// RFID/Alarm loop is exited when A3 switch reads HIGH, meaning the LED
// switch is in the ON position
// -------------------------------------------------------------------




// Begin LED while loop ----------------------------------------------
  while(analogRead(A3) > 1000){
    sensors_event_t event;
    lis.getEvent(&event);
  
    // Determine if bike is moving forwards or decellerating
    // If accelerometer outputs positive value, direction = forward
    if (event.acceleration.x > 0) {
      direction = true;
     }
    // If accelerometer reads negative value, bike is slowing down
    else{
      direction = false;
      }

    while (direction == true){
      Serial.println("detecting forward motion.");
      // [insert command for NORMAL light brightness here]
      for(int i=0; i<NUM_LEDS; i++) 
      {
        front_strip.setPixelColor(i, front_strip.Color(255, 255, 255)); // white for headlights
        back_strip.setPixelColor(i, back_strip.Color(150, 0, 0)); // red for taillights

      }
      back_strip.setBrightness(50); // Set brightness (0-255)
      front_strip.setBrightness(50); 
      front_strip.show(); // Update the strip to show the color
      back_strip.show(); //update the tail light to show color
      delay(1000);   // Wait for 1 second
      // Check again for change in acceleration direction
      // Determine if bike is moving forwards or decellerating
      lis.getEvent(&event);
      // If accelerometer outputs positive value, direction = forward
        if (event.acceleration.x > 0) {
          direction = true;
        }
        // If accelerometer reads negative value, bike is slowing down
        else{
          direction = false;
        }
    }

    while(direction == false){
      Serial.println("detecting forward motion.");
      for(int i=0; i<NUM_LEDS; i++) 
      {
        front_strip.setPixelColor(i, front_strip.Color(255, 255, 255)); // white for headlights
        back_strip.setPixelColor(i, back_strip.Color(150, 0, 0)); // red for taillights

      }
      back_strip.setBrightness(50); // Set brightness (0-255)
      front_strip.setBrightness(50); 
      front_strip.show(); // Update the strip to show the color
      back_strip.show(); //update the tail light to show color
      delay(1000);   // Wait for 1 second
      //[insert command for BRIGHT light brightness here]
      // Check again for change in acceleration direction
      // Determine if bike is moving forwards or decellerating
      // If accelerometer outputs positive value, direction = forward
        if (event.acceleration.x > 0) {
          direction = true;
        }
        // If accelerometer reads negative value, bike is slowing down
        else{
          direction = false;
        }
    }
  } // end LED while loop
  

  // Begin RFID loop -----------------------------------------------
  while(analogRead(A3) < 1000){

    //[insert code here to check is RFID is ARMED/DISARMED]


    //Alarm functionality loop - uncomment once RFID code is included
    // Begin Alarm loop --------------------------------------------
    //while(ARMED == true){
      // [insert alarm code here]

      // [Check again if RFID has been disarmed]
      // i.e. is ARMED true or false?
      // Exits alarm loop when ARMED is false and goes back to
      // doing nothing until A3 is HIGH or RFID is ARMED again
    // } // End Alarm while loop ------------------------------------

  } // End RFID while loop ------------------------------------

} // End void loop