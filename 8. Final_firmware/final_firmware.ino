#include "Arduino.h"
#include "AZ3166WiFi.h"

// IoT device SDK
#include "AzureIotHub.h"

// Sensors
#include "HTS221Sensor.h"
#include "LPS22HBSensor.h"
#include "RGB_LED.h"

// JSON library
#include "parson.h"

// Helper macros
#define randVal(min, max) (random(min, max))
#define RSV() (randVal(-2000, 2000))  // because I'm lazy RSV = Random Sensor Value

// debounce time for the A and B buttons
#define switchDebounceTime 250

// wifi settings and azure iot connection string
String ssid = F("AZUREIOTS");
String password = F("Azureiots1024");
String myConnStr = F("HostName=saas-iothub-6dd60f0a-4014-4d35-b1e2-d1e164920997.azure-devices.net;DeviceId=vzy8e8;SharedAccessKey=EwIZJbNHq1mfdJZyMa2B6ZeQSnrXmz+rEfCCza9U83I=");

// global Azure IoT Hub client handle
IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle;

// set to true if you want detailed logging from the Azure IoT device SDK
static const bool traceOn = false;

// set to true if you want to see payload details in the logging OUTPUT
static const bool payloadLogging = false;

// callback context definition
typedef struct EVENT_INSTANCE_TAG {
    IOTHUB_MESSAGE_HANDLE messageHandle;
    int messageTrackingId; // For tracking the messages within the user callback.
} EVENT_INSTANCE;

// counters for display and context
static int trackingId = 0;
static int errorCount = 0;
static int sentCount = 0;
static int ackCount = 0;

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



// this function is needed to yield to the hub SDK to process messages to and from the hub
void hubClientYield(void) {
    int waitTime = 1;
    IoTHubClient_LL_DoWork(iotHubClientHandle);
    ThreadAPI_Sleep(waitTime);
}

// callback to process confirmations from reported property device twin operations
static void deviceTwinConfirmationCallback(int status_code, void* userContextCallback) {
    Serial.printf("DeviceTwin CallBack: Status_code = %u\r\n", status_code);
}

// send reported properties to the IoT hub
bool sendReportedProperty(const char *payload) {
    bool retValue = true;
    
    IOTHUB_CLIENT_RESULT result = IoTHubClient_LL_SendReportedState(iotHubClientHandle, (const unsigned char*)payload, strlen(payload), deviceTwinConfirmationCallback, NULL);

    if (result != IOTHUB_CLIENT_OK) {
        Serial.println("Failure sending reported property!!!");
        retValue = false;
    }

    return retValue;
}

// callback to process device twin desired property changes
static void deviceTwinGetStateCallback(DEVICE_TWIN_UPDATE_STATE update_state, const unsigned char* payLoad, size_t size, void* userContextCallback) {
    if (payloadLogging) {
        Serial.println((char*)payLoad);
    }

    JSON_Value *root_value;
    root_value = json_parse_string((const char*)payLoad);
    float voltage = -1;
    int desiredVersion;

    if (DEVICE_TWIN_UPDATE_COMPLETE == update_state) {  // process a full desired properties payload sent during initial connection

        voltage = json_object_dotget_number(json_object(root_value), "desired.setVoltage.value");
        desiredVersion = json_object_dotget_number(json_object(root_value), "desired.$version");

    } else {  // process a partial/patch desired property payload
        voltage = json_object_dotget_number(json_object(root_value), "setVoltage.value");
        desiredVersion = json_object_get_number(json_object(root_value), "$version");
    }

    if (voltage > -1) {
        Serial.printf("voltage: %f\r\n", voltage);

        if (voltage <= 100) {
            //set RGB LED to green
            rgbLed.setColor(0, 255, 0);
        } else if (voltage > 100 && voltage <=200) {
            // set RGB LED to blue
            rgbLed.setColor(0, 0, 255);
        } else if (voltage > 200) {
            // set RGB LED to red
            rgbLed.setColor(255, 0, 0);
        }
    }

    // acknowledge the desired state
    char buff[1024];
    sprintf(buff, "{\"setVoltage\":{\"value\":%f, \"statusCode\":%d, \"status\":\"%s\", \"desiredVersion\":%d}}", voltage, 200, "completed", desiredVersion);
    if (payloadLogging) {
        Serial.println(buff);
    }
    sendReportedProperty(buff);
}

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

    // initialize the platform
    if (platform_init() != 0) {
        (void)Serial.printf("Failed to initialize the platform.\r\n");
        return;
    }

    // connect to the hub
    if ((iotHubClientHandle = IoTHubClient_LL_CreateFromConnectionString(myConnStr.c_str(), MQTT_Protocol)) == NULL) {
        (void)Serial.printf("ERROR: iotHubClientHandle is NULL!\r\n");
        return;
    } else {
        Screen.print(0, "Connected to hub");
    }

    // set some options
    IoTHubClient_LL_SetRetryPolicy(iotHubClientHandle, IOTHUB_CLIENT_RETRY_EXPONENTIAL_BACKOFF, 1200);
    IoTHubClient_LL_SetOption(iotHubClientHandle, "logtrace", &traceOn);
    if (IoTHubClient_LL_SetOption(iotHubClientHandle, "TrustedCerts", certificates) != IOTHUB_CLIENT_OK) {
        (void)Serial.printf("Failed to set option \"TrustedCerts\"\r\n");
        return;
    }

    // Setting twin call back, so we can receive desired properties. 
    if (IoTHubClient_LL_SetDeviceTwinCallback(iotHubClientHandle, deviceTwinGetStateCallback, NULL) != IOTHUB_CLIENT_OK) {
        (void)Serial.printf("ERROR: IoTHubClient_LL_SetDeviceTwinCallback..........FAILED!\r\n");
        return;
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

// callback to process telemetry acknowledgments from the hub
static void sendConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void *userContextCallback) {
    EVENT_INSTANCE *eventInstance = (EVENT_INSTANCE *)userContextCallback;

    (void)Serial.printf("Confirmation received for message tracking_id = %d with result = %s\r\n", eventInstance->messageTrackingId, ENUM_TO_STRING(IOTHUB_CLIENT_CONFIRMATION_RESULT, result));
    if (result == IOTHUB_CLIENT_CONFIRMATION_OK) {
        ackCount++;
    } else {
        errorCount++;
    }
    
    IoTHubMessage_Destroy(eventInstance->messageHandle);
    free(eventInstance);
}

// send a telemetry payload to the IoT Hub
bool sendTelemetry(const char *payload) {  
    IOTHUB_CLIENT_RESULT hubResult = IOTHUB_CLIENT_RESULT::IOTHUB_CLIENT_OK;

    // build the message from the passed in payload
    EVENT_INSTANCE *currentMessage = (EVENT_INSTANCE*)malloc(sizeof(EVENT_INSTANCE));
    currentMessage->messageHandle = IoTHubMessage_CreateFromByteArray((const unsigned char*)payload, strlen(payload));
    if (currentMessage->messageHandle == NULL) {
        (void)Serial.printf("ERROR: iotHubMessageHandle is NULL!\r\n");
        free(currentMessage);
        return false;
    }
    // add in the tracking id
    currentMessage->messageTrackingId = trackingId++;

    MAP_HANDLE propMap = IoTHubMessage_Properties(currentMessage->messageHandle);

    // add a timestamp to the message - illustrated for the use in batching
    time_t seconds = time(NULL);
    String temp = ctime(&seconds);
    temp.replace("\n","\0");
    if (Map_AddOrUpdate(propMap, "timestamp", temp.c_str()) != MAP_OK)
    {
        Serial.println("ERROR: Adding message property failed");
    }
    
    // submit the message to the Azure IoT hub
    hubResult = IoTHubClient_LL_SendEventAsync(iotHubClientHandle, currentMessage->messageHandle, sendConfirmationCallback, currentMessage);
    if (hubResult != IOTHUB_CLIENT_OK) {
        (void)Serial.printf("ERROR: IoTHubClient_LL_SendEventAsync..........FAILED (%s)!\r\n", hubResult);
        errorCount++;
        IoTHubMessage_Destroy(currentMessage->messageHandle);
        free(currentMessage);
        return false;
    } else {
        Serial.printf("IoTHubClient_LL_SendEventAsync accepted message for transmission to IoT Hub with tracking_id = %d\r\n", currentMessage->messageTrackingId);
    }

    // flash the Azure LED
    digitalWrite(LED_AZURE, 1);
    delay(500);
    digitalWrite(LED_AZURE, 0);

    return true;
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

// standard  Arduino loop function - called repeatedly for ever, think of this as the 
// event loop or message pump.  Try not to block this for long periods of time as your 
// code is single threaded.
void loop()
{
    // Send telemetry every 5 seconds
    if (millis() - lastTelemetrySend >= 5000) {
        // read sensors
        float temp = readTempSensor();
        float humidity = readHumiditySensor();
        float pressure = readPressureSensor();

        // build the JSON payload
        char payload[255];
        sprintf(payload, 
                "{\"humidity\": %f, \"temp\": %f, \"pressure\":%f, \"magnetometerX\": %d, \"magnetometerY\": %d, \"magnetometerZ\": %d, \"accelerometerX\": %d, \"accelerometerY\": %d, \"accelerometerZ\": %d, \"gyroscopeX\": %d, \"gyroscopeY\": %d, \"gyroscopeZ\": %d}",
                humidity, temp, pressure, RSV(), RSV(), RSV(), RSV(), RSV(), RSV(), RSV(), RSV(), RSV()
        );
        if (payloadLogging) {
            Serial.println(payload);
        }

        // send the telemetry
        if (sendTelemetry(payload)) {
            (void)Serial.printf("Send telemetry success\r\n");
            sentCount++;
        } else {
            (void)Serial.printf("Failed to send telemetry\r\n");
            errorCount++;
        }

        // display the send/ack/error stats on the display
        char buff[64];
        sprintf(buff, "sent: %d\r\nack: %d\r\nerror: %d", sentCount, ackCount, errorCount);
        Screen.print(0, buff);

        lastTelemetrySend = millis();
    }

    // Send an event when the user presses the A button
    if(digitalRead(USER_BUTTON_A) == LOW && (millis() - lastSwitchPress > switchDebounceTime)) {
        Serial.println("Button A pressed");

        // flash the user LED
        digitalWrite(LED_USER, 1);
        delay(500);
        digitalWrite(LED_USER, 0);

        // send the event
        char buff[1024];
        // get current time
        time_t seconds = time(NULL);
        String temp = ctime(&seconds);
        temp.replace("\n","\0");

        // build the event payload
        sprintf(buff, "{\"buttonA\": \"%s\"}", temp.c_str());
        if (payloadLogging) {
            Serial.println(buff);
        }

        // send the event - it's just a telemetry message
        if (sendTelemetry(buff)) {
            (void)Serial.printf("Send telemetry success\r\n");
            sentCount++;
        } else {
            (void)Serial.printf("Failed to send telemetry\r\n");
            errorCount++;
        }

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

        // send the reported property
        char buff[1024];

        // build the reported property JSON payload
        sprintf(buff, "{\"dieNumber\": %d}", dieNumber);
        if (payloadLogging) {
            Serial.println(buff);
        }

        // send the reported property
        sendReportedProperty(buff);

        lastSwitchPress = millis();
    }

    // yield to process any work to/from the hub
    hubClientYield();

    delay(1);  // need a minimum delay for stability
}
