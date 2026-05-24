#define ECL_HARDWARE_SERIAL_PORT 2
#define ECL_HARDWARE_SERIAL_RX 16
#define ECL_HARDWARE_SERIAL_TX 17
#define ECL_HARDWARE_SERIAL_SPEED 9600

#define ECL_ESPNOW_ENABLE

#include "ECL.h"
#include <map>

std::map<String, String> state;
String stationId;

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
            ECL::log.println("SYNC requested: sending state...");

            for (const auto &entry : state)
            {
                hardwareSerial.print(entry.first);
                hardwareSerial.print("|");
                hardwareSerial.println(entry.second);
            }

            hardwareSerial.println("SYSTEM|SYNC_COMPLETE");
            ECL::log.println("SYNC compleate!");
        }
        else if (incoming.indexOf('|') != -1)
        {
            int splitIdx = incoming.indexOf('|');
            String targetTopic = incoming.substring(0, splitIdx);
            String targetPayload = incoming.substring(splitIdx + 1);

            state[targetTopic] = targetPayload;

            ECL::publish(targetTopic.c_str(), targetPayload.c_str());

            ECL::log.printf("[FORWARD TO MESH] %s -> %s\n", targetTopic.c_str(), targetPayload.c_str());
        }
    }
}

void setup()
{
    ECL::begin();
    ECL::subscribe("#", eventCallback);
    stationId = WiFi.macAddress();
    state["system/mac"] = stationId;

    ECL::log.printf("Master Bridge Ready. MAC: %s\n", stationId.c_str());
}

void loop()
{
    ECL::loop();
    handleConnection();
}