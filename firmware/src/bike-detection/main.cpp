#define ECL_ESPNOW_ENABLE

#define TRIG_PIN D7
#define ECHO_PIN D8
#define MAX_DISTANCE 100 // in cm
#define EXPECTED_BIKE_DISTANCE 20 // in cm
#define BIKE_CHECK_DURATION 2500 // in ms
#define BIKE_CHECK_INTERVAL 500 // in ms

#include <NewPing.h>
#include <ECL.h>

#define SPACE_ID 0

NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE);

boolean has_bike() {
    for (int i = 0; i < BIKE_CHECK_DURATION / BIKE_CHECK_INTERVAL; i++) {
        ECL::log.println(itoa(sonar.ping_cm()));
        if (sonar.ping_cm() > EXPECTED_BIKE_DISTANCE) {
            return false;
        }
        delay(BIKE_CHECK_INTERVAL);
    }
    return true;
}

void check_for_bike() {
    bool bike_present = has_bike();
    String topic = "space/" + String(SPACE_ID) + "/bike";

    ECL::publish(topic.c_str(), bike_present ? "present" : "absent");
    ECL::log.println(bike_present ? "present" : "absent");
}

void setup()
{
    ECL::begin();
}

void loop()
{
    ECL::loop();
    check_for_bike();
    delay(1000);
}