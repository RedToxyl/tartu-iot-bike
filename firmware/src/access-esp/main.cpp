#include "ECL.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "keys.h"

#define ECL_ESPNOW_ENABLE

// =========================
// CONFIG
// =========================
static constexpr int N_SPACES = 5;
static constexpr int BIKE_TAKEOUT_TIME = 5000;
static constexpr int MAX_ACCESS_DELAY = 10000;

// =========================
// GLOBALS
// =========================
struct Space{
    String id;
    bool bikePresent;
    int last_access_attempt_time;
    String last_access_attempt_rfid;
}

String stationId = STATION_ID

Space spaces[N_SPACES];

// SETUP
// =========================

void initializeSpaces()
{
    for (int i = 0; i < N_SPACES; i++)
    {
        String spaceId = String(i);


        ECL::publish("api/create_space", "\"station\": \"" + stationId + "\", \"space\": \"" + spaceId + "\"");
        spaces[i] = {spaceId, false, 0, ""};
    }
}

void initalizeHandlers(){
    ECL::subscribe("api/return/#", validate_access);
    ECL::subscribe("space/#/bike", update_space);
    ECL::subscribe("space/#/rfid", attempt_access);
}

void setup()
{
    ECL::begin();

    delay(2000);

    stationId = WiFi.macAddress();

    ECL::log.printf(
        "My MAC address is [%s]\n",
        stationId.c_str());
    ECL::subscribe("space");
    ECL::subscribe("api")
    ECL::publish('api/create_station', "\"station\": \"" + stationId + "\", \"name\": \"" + STATION_NAME + "\", \"lat\": " + String(LATITUDE) + ", \"lon\": " + String(LONGITUDE) + "\"");
    initializeSpaces();
}

// =========================
// LOOP
// =========================

void loop()
{
    ECL::loop();
    ECL::publish('api/keep_alive', stationId.c_str());

    delay(1000);
}

// =========================
// TOPIC PARSING
// =========================

int topicToSpaceId(const char *topic)
{
    // expected:
    // space/0/update

    String topicStr(topic);

    int firstSlash = topicStr.indexOf('/');
    int secondSlash = topicStr.indexOf('/', firstSlash + 1);

    String idStr =
        topicStr.substring(
            firstSlash + 1,
            secondSlash);

    return idStr.toInt();
}

// =========================
// EVENT HANDLERS
// =========================
void allow_access(int spaceId){ // called when a valid rfid was presented and the server responded in time
    String spaceStr = String(spaceId);
    // unlock
    ECL::publish(
        "space/" + spaceStr + "/solenoid",
        "unlock");

    delay(BIKE_TAKEOUT_TIME);

    // lock
    ECL::publish(
        "space/" + spaceStr + "/solenoid",
        "lock");

    // update backend
    if (spaceId >= 0 &&
        spaceId < N_SPACES &&
        spaces[spaceId].bikePresent) // we're doing it this way so that bike-detection has a purpose
    {
        ECL::publish('api/lock', "\"station\": \"" + stationId + "\", \"space\": \"" + spaceStr + "\", \"rfid\": \"" + rfid + "\"");
        ECL::log.println(
            "Bike detected -> locked");
    }
    else
    {
        ECL::publish('api/unlock', "\"station\": \"" + stationId + "\", \"space\": \"" + spaceStr + "\", \"rfid\": \"" + rfid + "\"");
        ECL::log.println(
            "No bike -> unlocked");
    }
}


void validate_access(char *topic, char *payload) // called when the server responds with the stored rfid for a space
{
    JsonDocument spaceState;
    deserializeJson(spaceState, payload);

    int spaceId = spaceState["s.id"].as<int>();
    const char *rfid = spaceState["e.rfid"].as<const char *>();

    if (spaceId >= 0 && spaceId < N_SPACES)
    {
        int currentTime = millis();

        // keeping this in requires some extra counter, otherwise it loops forever
        /*if (currentTime - spaces[spaceId].last_access_attempt_time > MAX_ACCESS_DELAY) 
        {
            attempt_access(topic, payload);
        }*/

        if (currentTime - spaces[spaceId].last_access_attempt_time <= MAX_ACCESS_DELAY){ // still up to date
            if (strcmp(rfid, spaces[spaceId].last_access_attempt_rfid.c_str()) == 0){ // rfid matches db
                allow_access(spaceId);
            }
            else {
                ECL::log.println("RFID not authorized for this space");
            }
        }
    }
}

void attempt_access(char *topic, char *payload) // called when the user presents rfid
{
    int spaceId = topicToSpaceId(topic);
    String rfid(payload);

    if (spaceId >= 0 && spaceId < N_SPACES)
    {
        spaces[spaceId].last_access_attempt_time = millis();
        spaces[spaceId].last_access_attempt_rfid = String(payload);
    }
 
    ECL::publish('api/space', "\"station\": \"" + stationId + "\", \"space\": \"" + spaceStr + "\"");

    ECL::log.printf(
        "Request for space %d from RFID %s\n",
        spaceId,
        rfid.c_str());
}


void update_space(char *topic, char *payload)
{
    int spaceId = topicToSpaceId(topic);

    bool bikePresent =
        String(payload) == "present";

    if (spaceId >= 0 && spaceId < N_SPACES)
    {
        spaces[spaceId].bikePresent = bikePresent;
    }
}