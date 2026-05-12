#define ECL_SERIAL_SPEED 115200 // Enables Serial

#define ECL_WIFI_SSID "IOT15"         // Enables Wifi (needed for OTA, MQTT, or Telnet)
#define ECL_WIFI_PASSWORD "iotempire" // optional, default: "iotempire"

#define ECL_SOFTWARE_SERIAL_RX 16      // Enables SoftwareSerial
#define ECL_SOFTWARE_SERIAL_TX 17      // Enables SoftwareSerial
#define ECL_SOFTWARE_SERIAL_SPEED 9600 // optional, default: 9600
// #define ECL_SOFTWARE_SERIAL_ENABLE_LOGS // Enables SoftwareSerial for ECL::log.print

#define ECL_OTA_HOSTNAME "esp32-minikitA" // Enables OTA
#define ECL_OTA_PASSWORD "iotempower"     // optional, default: "iotempower"

#define ECL_TELNET_PORT 23 // Enables logs via Telnet

#define ECL_MQTT_SERVER "192.168.14.1"   // Enables MQTT
#define ECL_MQTT_CLIENT "esp32-minikitA" // optional, default: "node"
#define ECL_MQTT_PORT 1883               // optional, default: 1883

#include "ECL.h"
#include "Button.h"

ECL::Button btn(17);

void mqttCallback(char *topic, char *payload)
{
    ECL::log.printf("Received on [%s]: %s", topic, payload); // printf via Serial & Telnet
    softwareSerial.println(payload);                         // println via softwareSerial
}

void onBtnClick()
{
    static bool alarm = false;
    ECL::log.println("Button is pressed!");
    ECL::mqttPublish("security/alarm", (alarm = !alarm) ? "on" : "off");
}

void setup()
{
    ECL::begin(); // initializes OTA, MQTT, Telnet
    ECL::mqttSubscribe("prison/#", mqttCallback);
    btn.setOnPress(onBtnClick); // handles initialization and integration into ECL loop
    ECL::setInterval(3000, [](){ ECL::log.println("Timer!"); }); // invoke every 3000ms
}

void loop()
{
    ECL::loop(); // handles OTA, MQTT, Telnet Buttons etc
}