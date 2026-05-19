#define ECL_HARDWARE_SERIAL_PORT 2
#define ECL_HARDWARE_SERIAL_RX 16
#define ECL_HARDWARE_SERIAL_TX 17
#define ECL_HARDWARE_SERIAL_SPEED 9600
#define ECL_HARDWARE_SERIAL_ENABLE_LOGS

#define ECL_ESPNOW_ENABLE

#include "ECL.h"

void eventCallback(char *topic, char *payload)
{
    ECL::log.printf("%s|%s\n", topic, payload);
}

void setup()
{
    ECL::begin();
    ECL::subscribe("#", eventCallback);
}

void loop()
{
    ECL::loop();
}