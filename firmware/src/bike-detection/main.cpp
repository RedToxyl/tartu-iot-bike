#include <ECL.h>
#include <NewPing.h>

#define ECL_ESPNOW_ENABLE

#define TRIG_PIN D7
#define ECHO_PIN D8
#define MAX_DISTANCE 200 // in cm
#define EXPECTED_BIKE_DISTANCE 50 // in cm
#define BIKE_CHECK_DURATION 2500 // in ms
#define BIKE_CHECK_INTERVAL 500 // in ms

NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE);

boolean has_bike() {
    for (int i = 0; i < BIKE_CHECK_DURATION / BIKE_CHECK_INTERVAL; i++) {
        if (sonar.ping_cm() > EXPECTED_BIKE_DISTANCE) {
            return false;
        }
        delay(BIKE_CHECK_INTERVAL);
    }
    return true;
}

void check_for_bike(char *topic, char *payload) {
    bool bike_present = has_bike();
    ECL::publish("space/a/result", bike_present ? "present" : "absent");
}

void setup()
{
    ECL::begin();
    ECL::subscribe("space/a/check", check_for_bike);
}

void loop()
{
    ECL::loop();
    delay(150);
}