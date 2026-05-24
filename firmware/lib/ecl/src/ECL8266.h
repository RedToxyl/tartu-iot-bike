#pragma once
#if !defined(ESP8266)
#error "ECL8266.h targets ESP8266 boards. Use ECL.h on ESP32."
#endif

#include <Arduino.h>
#include <vector>

// ========== VALIDATION & DEPENDENCIES ==========
#if defined(ECL_OTA_HOSTNAME) || defined(ECL_MQTT_SERVER) || defined(ECL_TELNET_PORT)
#ifndef ECL_WIFI_SSID
#error "ECL Error: ECL_WIFI_SSID must be defined to use OTA, MQTT, or Telnet!"
#endif
#endif

// ========== WIFI GLOBALS ==========
#if defined(ECL_WIFI_SSID) || defined(ECL_ESPNOW_ENABLE)
#include <ESP8266WiFi.h>
#ifndef ECL_WIFI_PASSWORD
#define ECL_WIFI_PASSWORD "iotempire"
#endif
WiFiClient wifiClient;
#ifndef ECL_WIFI_CHANNEL
#define ECL_WIFI_CHANNEL 1
#endif
#endif

// ========== TELNET GLOBALS ==========
#if defined(ECL_TELNET_PORT)
WiFiServer telnetServer(ECL_TELNET_PORT);
WiFiClient telnetClient;
#endif

// ========== MQTT GLOBALS ==========
#if defined(ECL_MQTT_SERVER) || defined(ECL_ESPNOW_ENABLE)
struct MqttSubscription
{
    String topic;
    std::function<void(char *, byte *, unsigned int)> callback;
};
std::vector<MqttSubscription> _eclMqttSubscriptions;
#endif

#if defined(ECL_MQTT_SERVER)
#include <PubSubClient.h>
#ifndef ECL_MQTT_PORT
#define ECL_MQTT_PORT 1883
#endif
#ifndef ECL_MQTT_CLIENT
#define ECL_MQTT_CLIENT "node"
#endif
PubSubClient mqttClient(wifiClient);
#endif

// ========== ESP-NOW MESH GLOBALS ==========
#if defined(ECL_ESPNOW_ENABLE)
#include <espnow.h>
extern "C"
{
#include <user_interface.h>
}

struct EclEspNowMsg
{
    uint32_t nodeId;
    uint32_t msgId;
    char topic[64];
    char payload[170];
};
struct EclMsgQueueItem
{
    EclEspNowMsg msg;
    unsigned long processTime;
    bool needsRebroadcast;
    bool needsBroker;
};

const int MAX_QUEUE_SIZE = 15;
EclMsgQueueItem _eclMsgQueue[MAX_QUEUE_SIZE];
uint8_t _qHead = 0;
uint8_t _qTail = 0;

uint32_t _eclNodeId = 0;
uint32_t _eclMsgSeq = 0;
const int MAX_SEEN_MSGS = 30;
uint32_t _seenMsgs[MAX_SEEN_MSGS];
uint8_t _seenMsgIdx = 0;
uint8_t _broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

bool _isMsgSeen(uint32_t nId, uint32_t mId)
{
    uint64_t signature = ((uint64_t)nId << 32) | mId;
    for (int i = 0; i < MAX_SEEN_MSGS; i++)
    {
        if (_seenMsgs[i] == signature)
            return true;
    }
    return false;
}

void _markMsgSeen(uint32_t nId, uint32_t mId)
{
    uint64_t signature = ((uint64_t)nId << 32) | mId;
    _seenMsgs[_seenMsgIdx] = signature;
    _seenMsgIdx = (_seenMsgIdx + 1) % MAX_SEEN_MSGS;
}

void _espNowBroadcast(uint32_t nId, uint32_t mId, const char *topic, const char *payload)
{
    EclEspNowMsg msg;
    msg.nodeId = nId;
    msg.msgId = mId;
    strncpy(msg.topic, topic, sizeof(msg.topic) - 1);
    msg.topic[sizeof(msg.topic) - 1] = '\0';
    strncpy(msg.payload, payload, sizeof(msg.payload) - 1);
    msg.payload[sizeof(msg.payload) - 1] = '\0';

    esp_now_send(_broadcastAddress, (uint8_t *)&msg, sizeof(EclEspNowMsg));
}
#endif

// ========== OTA GLOBALS ==========
#if defined(ECL_OTA_HOSTNAME)
#include <ArduinoOTA.h>
#ifndef ECL_OTA_PASSWORD
#define ECL_OTA_PASSWORD "iotempower"
#endif
#endif

// ========== GLOBALS ==========
struct EclTimer
{
    unsigned long interval;
    unsigned long lastRun;
    std::function<void()> callback;
};
class Logger : public Print
{
public:
    size_t write(uint8_t c) override
    {
        size_t n = Serial.write(c);
#if defined(ECL_TELNET_PORT)
        if (telnetClient && telnetClient.connected())
            telnetClient.write(c);
#endif
        return n;
    }
};

std::vector<std::function<void()>> _eclLoopHandlers;
std::vector<EclTimer> _eclTimers;

// ========== LIBRARY NAMESPACE ==========
namespace ECL
{
    inline void addToLoop(std::function<void()> cb)
    {
        _eclLoopHandlers.push_back(cb);
    }
    inline void setInterval(unsigned long intervalMs, std::function<void()> cb)
    {
        _eclTimers.push_back({intervalMs, millis(), cb});
    }

    Logger log;

#if defined(ECL_MQTT_SERVER) || defined(ECL_ESPNOW_ENABLE)
    bool _mqttTopicMatch(const char *sub, const char *topic)
    {
        while (*sub && *topic)
        {
            if (*sub == '#')
                return true;
            if (*sub == '+')
            {
                while (*topic && *topic != '/')
                    topic++;
                sub++;
                if (*topic == '/' && *sub == '/')
                {
                    topic++;
                    sub++;
                }
                continue;
            }
            if (*sub != *topic)
                return false;
            sub++;
            topic++;
        }
        if (*sub == '#' && *(sub + 1) == '\0')
            return true;
        return (*sub == '\0' && *topic == '\0');
    }

    inline void _mqttRoute(char *topic, byte *payload, unsigned int length)
    {
        for (auto &sub : _eclMqttSubscriptions)
            if (_mqttTopicMatch(sub.topic.c_str(), topic))
                sub.callback(topic, payload, length);
    }

    inline void publish(const char *topic, const char *payload)
    {
#if defined(ECL_ESPNOW_ENABLE)
        _eclMsgSeq++;
        _markMsgSeen(_eclNodeId, _eclMsgSeq);
        _espNowBroadcast(_eclNodeId, _eclMsgSeq, topic, payload);
#endif

#if defined(ECL_MQTT_SERVER)
        if (mqttClient.connected())
            mqttClient.publish(topic, payload);
#endif
    }

    inline void mqttSubscribe(const char *topic, std::function<void(char *, byte *, unsigned int)> cb)
    {
        _eclMqttSubscriptions.push_back({topic, cb});
#if defined(ECL_MQTT_SERVER)
        if (mqttClient.connected())
            mqttClient.subscribe(topic);
#endif
    }

    inline void subscribe(const char *topic, std::function<void(char *, char *)> cb)
    {
        _eclMqttSubscriptions.push_back(
            {topic, [cb](char *t, byte *p, unsigned int len)
             {
                 String payloadStr;
                 payloadStr.reserve(len + 1);
                 for (unsigned int i = 0; i < len; i++)
                     payloadStr += (char)p[i];
                 cb(t, (char *)payloadStr.c_str());
             }});
#if defined(ECL_MQTT_SERVER)
        if (mqttClient.connected())
            mqttClient.subscribe(topic);
#endif
    }

    inline void _mqttBrokerCallback(char *topic, byte *payload, unsigned int length)
    {
#if defined(ECL_MQTT_SERVER)
        _mqttRoute(topic, payload, length);
#endif
#if defined(ECL_ESPNOW_GATEWAY) && defined(ECL_ESPNOW_ENABLE)
        char payloadStr[170] = {0};
        unsigned int cpLen = length < 169 ? length : 169;
        memcpy(payloadStr, payload, cpLen);

        _eclMsgSeq++;
        _markMsgSeen(_eclNodeId, _eclMsgSeq);
        _espNowBroadcast(_eclNodeId, _eclMsgSeq, topic, payloadStr);
#endif
    }
#endif

#if defined(ECL_ESPNOW_ENABLE)
    inline void _espNowOnRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len)
    {
        if (len != sizeof(EclEspNowMsg))
            return;
        // incomingData from the ESP-NOW SDK is not guaranteed 4-byte aligned;
        // copy into an aligned local before touching uint32_t fields, otherwise
        // the ESP8266 faults with Exception 9 (load/store alignment).
        EclEspNowMsg msg;
        memcpy(&msg, incomingData, sizeof(EclEspNowMsg));

        if (_isMsgSeen(msg.nodeId, msg.msgId))
            return;

        _markMsgSeen(msg.nodeId, msg.msgId);

        uint8_t nextHead = (_qHead + 1) % MAX_QUEUE_SIZE;
        if (nextHead != _qTail)
        {
            _eclMsgQueue[_qHead].msg = msg;
            _eclMsgQueue[_qHead].processTime = millis() + random(10, 75);
            _eclMsgQueue[_qHead].needsRebroadcast = true;

#if defined(ECL_ESPNOW_GATEWAY) && defined(ECL_MQTT_SERVER)
            _eclMsgQueue[_qHead].needsBroker = true;
#else
            _eclMsgQueue[_qHead].needsBroker = false;
#endif
            _qHead = nextHead;
        }
    }
#endif

    inline void begin()
    {
#if defined(ECL_SERIAL_SPEED)
        Serial.begin(ECL_SERIAL_SPEED);
#endif

#if defined(ECL_WIFI_SSID) || defined(ECL_ESPNOW_ENABLE)
        WiFi.mode(WIFI_STA);
        WiFi.setSleepMode(WIFI_NONE_SLEEP);
#endif

#if defined(ECL_WIFI_SSID)
        ECL::log.printf("Connecting to WiFi: %s\n", ECL_WIFI_SSID);
        WiFi.begin(ECL_WIFI_SSID, ECL_WIFI_PASSWORD);

#if defined(ECL_OTA_HOSTNAME)
        WiFi.hostname(ECL_OTA_HOSTNAME);
#endif
        while (WiFi.status() != WL_CONNECTED)
        {
            delay(500);
            Serial.print(".");
        }
        ECL::log.println("\nWiFi connected. IP: " + WiFi.localIP().toString());
#elif defined(ECL_ESPNOW_ENABLE)
        WiFi.disconnect();
#endif

#if defined(ECL_ESPNOW_ENABLE)
        if (esp_now_init() != 0)
        {
            ECL::log.println("Error initializing ESP-NOW");
            return;
        }

#if defined(ECL_WIFI_SSID)
        uint8_t channel = WiFi.channel();
#else
        uint8_t channel = ECL_WIFI_CHANNEL;
        wifi_set_channel(channel);
#endif

        _eclNodeId = (uint32_t)RANDOM_REG32;
        esp_now_set_self_role(ESP_NOW_ROLE_COMBO);

        if (esp_now_add_peer(_broadcastAddress, ESP_NOW_ROLE_SLAVE, channel, nullptr, 0) != 0)
        {
            ECL::log.println("Failed to add ESP-NOW peer");
            return;
        }

        esp_now_register_recv_cb(_espNowOnRecv);
        ECL::log.println("ESP-NOW Mesh Active.");
#endif

#if defined(ECL_TELNET_PORT)
        telnetServer.begin();
#endif

#if defined(ECL_MQTT_SERVER)
        mqttClient.setServer(ECL_MQTT_SERVER, ECL_MQTT_PORT);
        mqttClient.setCallback(_mqttBrokerCallback);
#endif

#if defined(ECL_OTA_HOSTNAME)
        ArduinoOTA.setHostname(ECL_OTA_HOSTNAME);
        ArduinoOTA.setPassword(ECL_OTA_PASSWORD);
        ArduinoOTA.onStart([]()
                           { ECL::log.println("OTA Start"); });
        ArduinoOTA.onEnd([]()
                         { ECL::log.println("\nOTA End"); });
        ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                              { ECL::log.printf("Progress: %u%%\r\n", (progress / (total / 100))); });
        ArduinoOTA.onError([](ota_error_t error)
                           { ECL::log.printf("Error[%u]\n", error); });
        ArduinoOTA.begin();
        ECL::log.println("OTA Ready");
#endif
    }

    inline void loop()
    {
#if defined(ECL_ESPNOW_ENABLE)
        while (_qTail != _qHead)
        {
            if (millis() >= _eclMsgQueue[_qTail].processTime)
            {
                EclEspNowMsg &m = _eclMsgQueue[_qTail].msg;
                _mqttRoute(m.topic, (byte *)m.payload, strlen(m.payload));
                if (_eclMsgQueue[_qTail].needsRebroadcast)
                    _espNowBroadcast(m.nodeId, m.msgId, m.topic, m.payload);
#if defined(ECL_ESPNOW_GATEWAY) && defined(ECL_MQTT_SERVER)
                if (_eclMsgQueue[_qTail].needsBroker && mqttClient.connected())
                    mqttClient.publish(m.topic, m.payload);
#endif
                _qTail = (_qTail + 1) % MAX_QUEUE_SIZE;
            }
            else
                break;
        }
#endif
#if defined(ECL_OTA_HOSTNAME)
        ArduinoOTA.handle();
#endif

#if defined(ECL_TELNET_PORT)
        if (telnetServer.hasClient())
            telnetClient = telnetServer.available();
#endif

#if defined(ECL_MQTT_SERVER)
        if (!mqttClient.connected())
        {
            if (mqttClient.connect(ECL_MQTT_CLIENT))
            {
                for (const auto &sub : _eclMqttSubscriptions)
                    mqttClient.subscribe(sub.topic.c_str());
                ECL::log.println("MQTT Reconnected & Subscribed");
            }
        }
        else
        {
            mqttClient.loop();
        }
#endif
        for (const auto &handler : _eclLoopHandlers)
            handler();

        unsigned long currentMillis = millis();
        size_t numTimers = _eclTimers.size();
        for (size_t i = 0; i < numTimers; i++)
        {
            if (currentMillis - _eclTimers[i].lastRun >= _eclTimers[i].interval)
            {
                _eclTimers[i].lastRun = currentMillis;
                if (_eclTimers[i].callback)
                    _eclTimers[i].callback();
            }
        }
    }
}
