#include "ECL.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "keys.h"

#define ECL_ESPNOW_ENABLE

// =========================
// CONFIG
// =========================

static const char *HOSTNAME = "https://iot.corebyte.ee";

static constexpr float LAT = 58.3776;
static constexpr float LON = 26.7290;

static const char *STATION_NAME = "Delta Station";

static constexpr int N_SPACES = 5;
static constexpr int BIKE_TAKEOUT_TIME = 5000;

// =========================
// GLOBALS
// =========================

String stationId;

bool spaces[N_SPACES];

// =========================
// FORWARD DECLARATIONS
// =========================

// setup helpers
void initializeSpaces();

// MQTT/event handlers
void update_space(char *topic, char *payload);
void user_requests_space(char *topic, char *payload);

// parsing
int topicToSpaceId(const char *topic);

// API
bool apiPost(const char *endpoint, const String &payload, String &response);

JsonDocument getSpaceState(const char *station, const char *space);

int keepAlive(const char *id);

int createStation(
    const char *id,
    const char *name,
    float lat,
    float lon);

int createSpace(
    const char *station,
    const char *space);

int lockSpace(
    const char *station,
    const char *space,
    const char *rfid);

int unlockSpace(
    const char *station,
    const char *space,
    const char *rfid);

// =========================
// SETUP
// =========================

void setup()
{
    ECL::begin();

    delay(2000);

    stationId = WiFi.macAddress();

    ECL::log.printf(
        "My MAC address is [%s]\n",
        stationId.c_str());

    createStation(
        stationId.c_str(),
        STATION_NAME,
        LAT,
        LON);

    initializeSpaces();
}

// =========================
// LOOP
// =========================

void loop()
{
    ECL::loop();

    keepAlive(stationId.c_str());

    delay(1000);
}

// =========================
// INITIALIZATION
// =========================

void initializeSpaces()
{
    for (int i = 0; i < N_SPACES; i++)
    {
        String spaceId = String(i);

        createSpace(
            stationId.c_str(),
            spaceId.c_str());

        spaces[i] = false;
    }
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

void update_space(char *topic, char *payload)
{
    int spaceId = topicToSpaceId(topic);

    bool bikePresent =
        String(payload) == "present";

    if (spaceId >= 0 && spaceId < N_SPACES)
    {
        spaces[spaceId] = bikePresent;
    }
}

void user_requests_space(char *topic, char *payload)
{
    String rfid(payload);

    int spaceId = topicToSpaceId(topic);

    ECL::log.printf(
        "Request for space %d from RFID %s\n",
        spaceId,
        rfid.c_str());

    String spaceStr = String(spaceId);

    JsonDocument spaceState =
        getSpaceState(
            stationId.c_str(),
            spaceStr.c_str());

    // authorization check
    if (spaceState["rfid"] != rfid)
    {
        ECL::log.println(
            "Unauthorized RFID access");

        return;
    }

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
        spaces[spaceId])
    {
        lockSpace(
            stationId.c_str(),
            spaceStr.c_str(),
            rfid.c_str());

        ECL::log.println(
            "Bike detected -> locked");
    }
    else
    {
        unlockSpace(
            stationId.c_str(),
            spaceStr.c_str(),
            rfid.c_str());

        ECL::log.println(
            "No bike -> unlocked");
    }
}

// =========================
// HTTP HELPERS
// =========================

bool apiPost(
    const char *endpoint,
    const String &payload,
    String &response)
{
    HTTPClient http;

    http.begin(
        String(HOSTNAME) + endpoint);

    http.addHeader(
        "Content-Type",
        "application/json");

    http.addHeader(
        "Authorization",
        TOKEN1);

    int httpCode =
        http.POST(payload);

    response = http.getString();

    http.end();

    return httpCode > 0;
}

// =========================
// API
// =========================

JsonDocument getSpaceState(
    const char *station,
    const char *space)
{
    DynamicJsonDocument doc(1024);

    String response;

    String payload =
        "{\"station\":\"" +
        String(station) +
        "\",\"space\":\"" +
        String(space) +
        "\"}";

    apiPost(
        "/api/space",
        payload,
        response);

    deserializeJson(doc, response);

    return doc;
}

int keepAlive(const char *id)
{
    String response;

    String payload =
        "{\"id\":\"" +
        String(id) +
        "\"}";

    apiPost(
        "/api/keep_alive",
        payload,
        response);

    return 0;
}

int createStation(
    const char *id,
    const char *name,
    float lat,
    float lon)
{
    String response;

    String payload =
        "{\"id\":\"" +
        String(id) +
        "\",\"name\":\"" +
        String(name) +
        "\",\"lat\":" +
        String(lat) +
        ",\"lon\":" +
        String(lon) +
        "}";

    apiPost(
        "/api/create_station",
        payload,
        response);

    return 0;
}

int createSpace(
    const char *station,
    const char *space)
{
    String response;

    String payload =
        "{\"station\":\"" +
        String(station) +
        "\",\"space\":\"" +
        String(space) +
        "\"}";

    apiPost(
        "/api/create_space",
        payload,
        response);

    return 0;
}

int lockSpace(
    const char *station,
    const char *space,
    const char *rfid)
{
    String response;

    String payload =
        "{\"station\":\"" +
        String(station) +
        "\",\"space\":\"" +
        String(space) +
        "\",\"rfid\":\"" +
        String(rfid) +
        "\"}";

    apiPost(
        "/api/lock",
        payload,
        response);

    return 0;
}

int unlockSpace(
    const char *station,
    const char *space,
    const char *rfid)
{
    String response;

    String payload =
        "{\"station\":\"" +
        String(station) +
        "\",\"space\":\"" +
        String(space) +
        "\",\"rfid\":\"" +
        String(rfid) +
        "\"}";

    apiPost(
        "/api/unlock",
        payload,
        response);

    return 0;
}