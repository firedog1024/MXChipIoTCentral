#include "Arduino.h"
#include "AZ3166WiFi.h"

// Sensors
#include "HTS221Sensor.h"
#include "LPS22HBSensor.h"
#include "RGB_LED.h"

// Helper macros
#define randVal(min, max) (random(min, max))
#define RSV() (randVal(-2000, 2000))  // because I'm lazy RSV = Random Sensor Value

// debounce time for the A and B buttons
#define switchDebounceTime 250

// wifi settings and azure iot connection string
String ssid = F("AZUREIOTS");
String password = F("Azureiots1024");

// are we connected to wifi
static bool connected = false;

// last action timers for the main loop
unsigned long lastTelemetrySend = 0;
unsigned long lastSwitchPress = 0;

// sensor variables
static DevI2C *i2c;
static LPS22HBSensor *pressure;
static HTS221Sensor *tempHumidity;
static RGB_LED rgbLed;

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

    // seed the pseudo-random number generator
    randomSeed(analogRead(0));

    // start the wifi
    if(WiFi.begin((char*)ssid.c_str(), (char*)password.c_str()) == WL_CONNECTED) {
        digitalWrite(LED_WIFI, 1);
        connected = true;
        Screen.print(0, "wifi connected");
    }

    // init i2c for reading sensors
    i2c = new DevI2C(D14, D15);

    // init sensors
    tempHumidity = new HTS221Sensor(*i2c);
    tempHumidity->init(NULL);

    pressure = new LPS22HBSensor(*i2c);
    pressure->init(NULL);

    // init buttons as input
    pinMode(USER_BUTTON_A, INPUT);
    pinMode(USER_BUTTON_B, INPUT);
}

// read temperature sensor
float readTempSensor() {
    float tempValue;
    tempHumidity->reset();
    if (tempHumidity->getTemperature(&tempValue) == 0)
        return tempValue;
    else
        return 0xFFFF;
}

// read humidity sensor
float readHumiditySensor() {
    float humidityValue;
    tempHumidity->reset();
    if (tempHumidity->getHumidity(&humidityValue) == 0)
        return humidityValue;
    else
        return 0xFFFF;
}

// read pressure sensor
float readPressureSensor() {
    float pressureValue;
    if (pressure->getPressure(&pressureValue) == 0)
        return pressureValue;
    else
        return 0xFFFF;
}

// standard  Arduino loop function - called repeatedly for ever, think of this as the event loop or message pump.
// try not to block this for long periods of time as your code is single threaded.
void loop()
{
    // Send telemetry every 5 seconds
    if (millis() - lastTelemetrySend >= 5000) {
        // read the sensors
        float temp = readTempSensor();
        float humidity = readHumiditySensor();
        float pressure = readPressureSensor();

        Serial.printf("Temperature: %f, humidity: %f, pressure: %f\r\n", temp, humidity, pressure);
        lastTelemetrySend = millis();
    }

    // Send an event when the user presses the A button
    if(digitalRead(USER_BUTTON_A) == LOW && (millis() - lastSwitchPress > switchDebounceTime)) {
        Serial.println("Button A pressed");

        // flash the user LED
        digitalWrite(LED_USER, 1);
        delay(500);
        digitalWrite(LED_USER, 0);

        lastSwitchPress = millis();
    }

    // Send the reported property DieNumber when the user presses the B button
    if(digitalRead(USER_BUTTON_B) == LOW && (millis() - lastSwitchPress > switchDebounceTime)) {
        Serial.println("Button B pressed");

        // flash the user LED
        digitalWrite(LED_USER, 1);
        delay(500);
        digitalWrite(LED_USER, 0);

        // generate the die roll
        int dieNumber = randVal(1, 6);
        Serial.printf("You rolled a %d\r\n", dieNumber);

        lastSwitchPress = millis();
    }

    delay(1);  // need a minimum delay for stability
}