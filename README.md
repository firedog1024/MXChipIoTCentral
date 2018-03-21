# MXChipIoTCentral

MXChip for Azure IoT Central Tutorial code

This code builds on each previous sample to build a fully functioning firmware module for the MXChip that can send data to 
Azure IoT Central.

1. Mr_Bones - Just the bare minimum skeleton to create an MXChip application
2. Interwebz - Adding support for connecting to WiFi
3. Sensors - Reading the temperature, humidity, and pressure sensors
4. IotHub_telemetry - Sending the sensor data to IoT Central
5. IotHub_events - Sending event data to IoT Central when button A is pressed
6. IotHub_properties - Sending a reported property to IoT Central when button B is pressed
7. IotHub_settings - Recieving a setting (desired property) from IoT Central
8. Final_firmware - The complete firmware code (should be almost identical to 7. IotHub_settings) includes ability to build a drag and drop binary
