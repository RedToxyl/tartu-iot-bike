#define ECL_HARDWARE_SERIAL_PORT 2
#define ECL_HARDWARE_SERIAL_RX 16
#define ECL_HARDWARE_SERIAL_TX 17
#define ECL_HARDWARE_SERIAL_SPEED 9600

#define ECL_ESPNOW_ENABLE

#include "ECL.h"
#include <ArduinoJson.h>
#include <map>

// =========================
// CONFIG
// =========================
static constexpr int N_SPACES = 5;
static constexpr int BIKE_TAKEOUT_TIME = 5000;
static constexpr int MAX_ACCESS_DELAY = 10000;

static const char *STATION_NAME = "Delta Station";
static constexpr float LATITUDE = 58.3776;
static constexpr float LONGITUDE = 26.7290;

// =========================
// GLOBALS
// =========================
struct Space
{
    String id;
    bool bikePresent;
    int last_access_attempt_time;
    String last_access_attempt_rfid;
};

String stationId;
std::map<String, String> state;
Space spaces[N_SPACES];

void validate_access(char *topic, char *payload);
void update_space(char *topic, char *payload);
void attempt_access(char *topic, char *payload);
int topicToSpaceId(const char *topic);

// =========================
// HARDWARE BRIDGE LOGIC
// =========================
void eventCallback(String topic, String payload)
{
    state[topic] = payload;
    ECL::log.printf("[EVENT] %s %s\n", topic.c_str(), payload.c_str());

    if (topic.startsWith("api/"))
    {
        ECL::log.println("redirect via bluetooth");
        hardwareSerial.print(topic);
        hardwareSerial.print("|");
        hardwareSerial.println(payload);
    }
}

void handleConnection()
{
    if (hardwareSerial.available())
    {
        String incoming = hardwareSerial.readStringUntil('\n');
        incoming.trim();

        if (incoming == "SYNC")
        {
            ECL::log.println("SYNC requested: sending api state...");

            for (const auto &entry : state)
            {
                if (entry.first.startsWith("api/"))
                {
                    hardwareSerial.print(entry.first);
                    hardwareSerial.print("|");
                    hardwareSerial.println(entry.second);
                }
            }
            hardwareSerial.println("SYSTEM|SYNC_COMPLETE");
            ECL::log.println("SYNC complete!");
        }
        else if (incoming.indexOf('|') != -1)
        {
            int splitIdx = incoming.indexOf('|');
            String targetTopic = incoming.substring(0, splitIdx);
            String targetPayload = incoming.substring(splitIdx + 1);

            state[targetTopic] = targetPayload;
            ECL::publish(targetTopic.c_str(), targetPayload.c_str());

            ECL::log.printf("[EXTERNAL] %s -> %s\n", targetTopic.c_str(), targetPayload.c_str());
        }
    }
}

// =========================
// STATION LOGIC
// =========================
void initializeSpaces()
{
    for (int i = 0; i < N_SPACES; i++)
    {
        String spaceId = String(i);
        String createSpacePayload = "{\"station\": \"" + stationId + "\", \"space\": \"" + spaceId + "\"}";

        eventCallback("api/create_space", createSpacePayload);
        spaces[i] = {spaceId, false, 0, ""};
    }
}

void initializeHandlers()
{
    ECL::subscribe("api/return/#", validate_access);
    ECL::subscribe("space/+/bike", update_space);
    ECL::subscribe("space/+/rfid", attempt_access);
}

void keepAlive()
{
    String payload = "{\"id\": \"" + stationId + "\"}";
    eventCallback("api/keep_alive", payload); // void eventCallback(char *topic, char *payload)
}

void allow_access(int spaceId)
{
    String spaceStr = String(spaceId);
    String unlockTopic = "space/" + spaceStr + "/solenoid";
    ECL::publish(unlockTopic.c_str(), "unlock");

    delay(BIKE_TAKEOUT_TIME);

    String lockTopic = "space/" + spaceStr + "/solenoid";
    ECL::publish(lockTopic.c_str(), "lock");

    String attemptRfid = "";
    if (spaceId >= 0 && spaceId < N_SPACES)
    {
        attemptRfid = spaces[spaceId].last_access_attempt_rfid;
    }

    String payload = "{\"station\": \"" + stationId + "\", \"space\": \"" + spaceStr + "\", \"rfid\": \"" + attemptRfid + "\"}";

    if (spaceId >= 0 && spaceId < N_SPACES && spaces[spaceId].bikePresent)
    {
        eventCallback("api/lock", payload);
        ECL::log.println("Bike detected -> locked");
    }
    else
    {
        eventCallback("api/unlock", payload);
        ECL::log.println("No bike -> unlocked");
    }
}

void validate_access(char *topic, char *payload)
{
    DynamicJsonDocument spaceState(256);
    deserializeJson(spaceState, payload);

    int spaceId = spaceState["s.id"].as<int>();
    const char *rfid = spaceState["e.rfid"].as<const char *>();

    if (spaceId >= 0 && spaceId < N_SPACES)
    {
        int currentTime = millis();

        if (currentTime - spaces[spaceId].last_access_attempt_time <= MAX_ACCESS_DELAY)
        {
            if (strcmp(rfid, spaces[spaceId].last_access_attempt_rfid.c_str()) == 0 || strcmp(rfid, "ACCESS-FREE") == 0)
            {
                allow_access(spaceId);
            }
            else
            {
                ECL::log.println("RFID not authorized for this space");
            }
        }
    }
}

void attempt_access(char *topic, char *payload)
{
    int spaceId = topicToSpaceId(topic);
    String rfid(payload);
    String spaceStr = String(spaceId);

    if (spaceId >= 0 && spaceId < N_SPACES)
    {
        spaces[spaceId].last_access_attempt_time = millis();
        spaces[spaceId].last_access_attempt_rfid = String(payload);
    }

    String payloadStr = "{\"station\": \"" + stationId + "\", \"space\": \"" + spaceStr + "\"}";
    eventCallback("api/space", payloadStr);
    ECL::log.printf("Request for space %d from RFID %s\n", spaceId, rfid.c_str());
}

void update_space(char *topic, char *payload)
{
    int spaceId = topicToSpaceId(topic);
    if (spaceId >= 0 && spaceId < N_SPACES)
    {
        spaces[spaceId].bikePresent = (String(payload) == "present");
    }
}

int topicToSpaceId(const char *topic)
{
    String topicStr(topic);
    int firstSlash = topicStr.indexOf('/');
    int secondSlash = topicStr.indexOf('/', firstSlash + 1);
    return topicStr.substring(firstSlash + 1, secondSlash).toInt();
}

// =========================
// LIFECYCLE
// =========================
void setup()
{
    ECL::begin();
    ECL::subscribe("#", eventCallback);

    stationId = WiFi.macAddress();
    ECL::log.printf("Master Bridge Ready. MAC: %s\n", stationId.c_str());

    initializeHandlers();

    String createStationPayload = "{\"id\": \"" + stationId + "\", \"name\": \"" + String(STATION_NAME) + "\", \"lat\": " + String(LATITUDE) + ", \"lon\": " + String(LONGITUDE) + "}";
    eventCallback("api/create_station", createStationPayload);

    initializeSpaces();

    ECL::setInterval(5000, keepAlive);
}

void loop()
{
    ECL::loop();
    handleConnection();
}