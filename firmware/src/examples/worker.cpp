#define ECL_ESPNOW_ENABLE

#include "ECL.h"
#include "Button.h"

ECL::Button btn(17);

void onBtnClick()
{
    static bool alarm = false;
    ECL::log.println("Worker button is pressed!");
    ECL::publish("btn", (alarm = !alarm) ? "on" : "off");
}

void setup()
{
    ECL::begin();
    btn.setOnPress(onBtnClick);
}

void loop()
{
    ECL::loop();
}