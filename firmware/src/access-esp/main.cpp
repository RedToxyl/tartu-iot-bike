#define ECL_ESPNOW_ENABLE

#include "ECL.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "keys.h"

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
};

String stationId;

void validate_access(char *topic, char *payload);
void update_space(char *topic, char *payload);
void attempt_access(char *topic, char *payload);

Space spaces[N_SPACES];

// SETUP
// =========================

void initializeSpaces()
{
    for (int i = 0; i < N_SPACES; i++)
    {
        String spaceId = String(i);
        String createSpacePayload = "\"station\": \"" + stationId + "\", \"space\": \"" + spaceId + "\"";
        ECL::publish("api/create_space", createSpacePayload.c_str());
        ECL::log.printf("PUBLISH %s %s\n", "api/create_space", createSpacePayload.c_str());
        spaces[i] = {spaceId, false, 0, ""};
    }
}

void initializeHandlers(){
    ECL::subscribe("api/return/#", validate_access);
    ECL::subscribe("space/+/bike", update_space);
    ECL::subscribe("space/+/rfid", attempt_access);
};

void setup()
{
    ECL::begin();

    delay(500);

    stationId = WiFi.macAddress();

    ECL::log.printf(
        "My MAC address is [%s]\n",
        stationId.c_str());
    initializeHandlers();
    String createStationPayload = "\"station\": \"" + stationId + "\", \"name\": \"" + STATION_NAME + "\", \"lat\": " + String(LATITUDE) + ", \"lon\": " + String(LONGITUDE) + "\"";
    ECL::publish("api/create_station", createStationPayload.c_str());
    ECL::log.printf("PUBLISH %s %s\n", "api/create_station", createStationPayload.c_str());
    initializeSpaces();
}

// =========================
// LOOP
// =========================

void loop()
{
    ECL::loop();
    
    ECL::publish("api/keep_alive", stationId.c_str());
    ECL::log.printf("PUBLISH %s %s\n", "api/keep_alive", stationId.c_str());

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
    String unlockTopic = "space/" + spaceStr + "/solenoid";
    ECL::publish(unlockTopic.c_str(), "unlock");
    ECL::log.printf("PUBLISH %s %s\n", unlockTopic.c_str(), "unlock");

    delay(BIKE_TAKEOUT_TIME);

    String lockTopic = "space/" + spaceStr + "/solenoid";
    ECL::publish(lockTopic.c_str(), "lock");
    ECL::log.printf("PUBLISH %s %s\n", lockTopic.c_str(), "lock");

    // update backend
    String attemptRfid = "";
    if (spaceId >= 0 && spaceId < N_SPACES)
    {
        attemptRfid = spaces[spaceId].last_access_attempt_rfid;
    }

    String payload = "\"station\": \"" + stationId + "\", \"space\": \"" + spaceStr + "\", \"rfid\": \"" + attemptRfid + "\"";
    if (spaceId >= 0 &&
        spaceId < N_SPACES &&
        spaces[spaceId].bikePresent) // we're doing it this way so that bike-detection has a purpose
    {
        ECL::publish("api/lock", payload.c_str());
        ECL::log.printf("PUBLISH %s %s\n", "api/lock", payload.c_str());
        ECL::log.println(
            "Bike detected -> locked");
    }
    else
    {
        ECL::publish("api/unlock", payload.c_str());
        ECL::log.printf("PUBLISH %s %s\n", "api/unlock", payload.c_str());
        ECL::log.println(
            "No bike -> unlocked");
    }
}

void validate_access(char *topic, char *payload) // called when the server responds with the stored rfid for a space
{
    DynamicJsonDocument spaceState(256);
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
            else if (strcmp(rfid, "ACCESS-FREE") == 0) {
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
    String spaceStr = String(spaceId);

    if (spaceId >= 0 && spaceId < N_SPACES)
    {
        spaces[spaceId].last_access_attempt_time = millis();
        spaces[spaceId].last_access_attempt_rfid = String(payload);
    }
 
    String payloadStr = "\"station\": \"" + stationId + "\", \"space\": \"" + spaceStr + "\"";
    ECL::publish("api/space", payloadStr.c_str());
    ECL::log.printf("PUBLISH %s %s\n", "api/space", payloadStr.c_str());

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