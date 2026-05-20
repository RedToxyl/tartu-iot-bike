#define ECL_HARDWARE_SERIAL_PORT 2
#define ECL_HARDWARE_SERIAL_RX 16
#define ECL_HARDWARE_SERIAL_TX 17
#define ECL_HARDWARE_SERIAL_SPEED 9600
// #define ECL_HARDWARE_SERIAL_ENABLE_LOGS

#define ECL_ESPNOW_ENABLE

#include "ECL.h"
#include <map>

std::map<String, String> state;

void eventCallback(char *topic, char *payload)
{
    state[String(topic)] = String(payload);
    ECL::log.printf("[EVENT] %s %s\n", topic, payload);
    hardwareSerial.print(topic);
    hardwareSerial.print("|");
    hardwareSerial.println(payload);
}

void handleConnection()
{
    if (hardwareSerial.available())
    {
        String incoming = hardwareSerial.readStringUntil('\n');
        incoming.trim();

        if (incoming == "SYNC")
        {
            ECL::log.println("Sending full state sync...");

            for (const auto &entry : state)
            {
                const auto &savedTopic = entry.first;
                const auto &savedPayload = entry.second;

                hardwareSerial.print(savedTopic);
                hardwareSerial.print("|");
                hardwareSerial.println(savedPayload);
            }

            ECL::log.println("SYSTEM|SYNC_COMPLETE");
        }
    }
}

void setup()
{
    ECL::begin();
    ECL::subscribe("#", eventCallback);
}

void loop()
{
    ECL::loop();
    handleConnection();
}