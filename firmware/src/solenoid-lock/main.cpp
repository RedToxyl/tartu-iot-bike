#include "ECL.h"

constexpr uint8_t RELAY_PIN = 32;
constexpr uint8_t RELAY_ON = LOW;
constexpr uint8_t RELAY_OFF = HIGH;
constexpr unsigned long MAX_UNLOCK_MS = 15000;

constexpr const char *CMD_TOPIC = "bike/lock/cmd";
constexpr const char *STATE_TOPIC = "bike/lock/state";

bool unlocked = false;
unsigned long unlockStart = 0;

void publishState()
{
    ECL::mqttPublish(STATE_TOPIC, unlocked ? "unlocked" : "locked");
}

void lock()
{
    digitalWrite(RELAY_PIN, RELAY_OFF);
    unlocked = false;
    ECL::log.println("Solenoid: locked");
    publishState();
}

void unlock()
{
    digitalWrite(RELAY_PIN, RELAY_ON);
    unlocked = true;
    unlockStart = millis();
    ECL::log.println("Solenoid: unlocked");
    publishState();
}

void setup()
{
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, RELAY_OFF);

    ECL::begin();

    ECL::mqttSubscribe(CMD_TOPIC, [](char *topic, char *payload)
    {
        if (strcmp(payload, "unlock") == 0)
            unlock();
        else if (strcmp(payload, "lock") == 0)
            lock();
        else
            ECL::log.printf("Solenoid: unknown command [%s]\n", payload);
    });
}

void loop()
{
    ECL::loop();

    // avoid overheating the lock
    if (unlocked && millis() - unlockStart >= MAX_UNLOCK_MS)
    {
        ECL::log.println("Solenoid: unlock timeout reached, forcing lock");
        lock();
    }
}
