#include "Arduino.h"

// standard  Arduino setup function - called once whent he device initializes on power up
void setup()
{
    // set the serial baud rate
	Serial.begin(250000);

    Serial.println("Just getting things setup");
}

// standard  Arduino loop function - called repeatedly for ever, think of this as the 
// event loop or message pump.  Try not to block this for long periods of time as your 
// code is single threaded.
void loop()
{
    Serial.println("nothing happening here");

    delay(1);  // need a minimum delay for stability
}