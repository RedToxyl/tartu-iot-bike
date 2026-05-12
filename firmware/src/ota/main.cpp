#include "ECL.h"

void setup()
{
    ECL::begin(); // initializes OTA, MQTT, Telnet
}

void loop()
{
    ECL::loop(); // handles OTA, MQTT, Telnet Buttons etc
}