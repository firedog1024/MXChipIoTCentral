#include "Arduino.h"
#include "AZ3166WiFi.h"

// wifi settings and azure iot connection string
String ssid = F("AZUREIOTS");
String password = F("Azureiots1024");

// are we connected to wifi
static bool connected = false;

// standard  Arduino setup function - called once whent he device initializes on power up
void setup()
{
    // set the serial baud rate
	Serial.begin(250000);

    // init the status LED's
    pinMode(LED_WIFI, OUTPUT);
    pinMode(LED_AZURE, OUTPUT);
    pinMode(LED_USER, OUTPUT);

    Screen.clean();

    // start the wifi
    if(WiFi.begin((char*)ssid.c_str(), (char*)password.c_str()) == WL_CONNECTED) {
        digitalWrite(LED_WIFI, 1);
        connected = true;
        Screen.print(0, "wifi connected");
    }
}

// standard  Arduino loop function - called repeatedly for ever, think of this as the 
// event loop or message pump.  Try not to block this for long periods of time as your 
// code is single threaded.
void loop()
{
    Serial.println("nothing happening here");

    delay(1);  // need a minimum delay for stability
}