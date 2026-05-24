#define ECL_ESPNOW_ENABLE

#include "ECL8266.h"

constexpr uint8_t RELAY_PIN = D5;
constexpr uint8_t RELAY_ON = LOW;
constexpr uint8_t RELAY_OFF = HIGH;
constexpr unsigned long MAX_LOCK_MS = 15000;

constexpr const char* CMD_TOPIC = "space/" SPACE_ID "/solenoid";
constexpr const char* STATE_TOPIC = "space/" SPACE_ID "/solenoid/state";

bool locked = false;
unsigned long lockStart = 0;

void publishState()
{
    ECL::publish(STATE_TOPIC, unlocked ? "unlocked" : "locked");
}

void lock()
{
    digitalWrite(RELAY_PIN, RELAY_OFF);
    locked = true;
    lockStart = millis();
    ECL::log.println("Solenoid: locked");
    publishState();
}

void unlock()
{
    digitalWrite(RELAY_PIN, RELAY_ON);
    locked = false;
    ECL::log.println("Solenoid: unlocked");
    publishState();
}

void onCmd(char *topic, char *payload)
{
    if (strcmp(payload, "unlock") == 0)
        unlock();
    else if (strcmp(payload, "lock") == 0)
        lock();
    else
        ECL::log.printf("Solenoid: unknown command [%s]\n", payload);
}

void setup()
{
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, RELAY_OFF);

    ECL::begin();
    ECL::subscribe(CMD_TOPIC, onCmd);
}

void loop()
{
    ECL::loop();

    // avoid overheating the lock
    if (locked && millis() - lockStart >= MAX_LOCK_MS)
    {
        ECL::log.println("Solenoid: lock timeout reached, forcing lock");
        unlock();
    }
}
