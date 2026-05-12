#pragma once
#include <Arduino.h>
#include <vector>

// ========== VALIDATION & DEPENDENCIES ==========
#if defined(ECL_OTA_HOSTNAME) || defined(ECL_MQTT_SERVER) || defined(ECL_TELNET_PORT)
#ifndef ECL_WIFI_SSID
#error "ECL Error: ECL_WIFI_SSID must be defined to use OTA, MQTT, or Telnet!"
#endif
#endif

// ========== WIFI GLOBALS ==========
#if defined(ECL_WIFI_SSID)

#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif

#ifndef ECL_WIFI_PASSWORD
#define ECL_WIFI_PASSWORD "iotempire"
#endif

WiFiClient wifiClient;

#endif

// ========== SOFTWARE SERIAL GLOBALS ==========
#if defined(ECL_SOFTWARE_SERIAL_RX) && defined(ECL_SOFTWARE_SERIAL_TX)
#include <SoftwareSerial.h>
#ifndef ECL_SOFTWARE_SERIAL_SPEED
#define ECL_SOFTWARE_SERIAL_SPEED 9600
#endif
SoftwareSerial softwareSerial(ECL_SOFTWARE_SERIAL_RX, ECL_SOFTWARE_SERIAL_TX);
#endif

// ========== TELNET GLOBALS ==========
#if defined(ECL_SOFTWARE_SERIAL_ENABLE_LOGS)
#if !defined(ECL_SOFTWARE_SERIAL_RX) || !defined(ECL_SOFTWARE_SERIAL_TX)
#error "ECL Error: Software Serial logs require both ECL_SOFTWARE_SERIAL_RX and ECL_SOFTWARE_SERIAL_TX!"
#endif
#endif

#if defined(ECL_TELNET_PORT)
WiFiServer telnetServer(ECL_TELNET_PORT);
WiFiClient telnetClient;
#endif

// ========== MQTT GLOBALS ==========
#if defined(ECL_MQTT_SERVER)
#include <PubSubClient.h>
#ifndef ECL_MQTT_PORT
#define ECL_MQTT_PORT 1883
#endif
#ifndef ECL_MQTT_CLIENT
#define ECL_MQTT_CLIENT "node"
#endif
PubSubClient mqttClient(wifiClient);

struct MqttSubscription
{
    String topic;
    std::function<void(char *, byte *, unsigned int)> callback;
};

std::vector<MqttSubscription> _eclMqttSubscriptions;
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

#if defined(ECL_SOFTWARE_SERIAL_ENABLE_LOGS)
        softwareSerial.write(c);
#endif

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

#if defined(ECL_MQTT_SERVER)

    bool _mqttTopicMatch(const char *sub, const char *topic)
    {
        while (*sub && *topic)
        {
            if (*sub == '#') return true;

            if (*sub == '+')
            {
                while (*topic && *topic != '/') topic++;
                sub++;
                if (*topic == '/' && *sub == '/')
                {
                    topic++;
                    sub++;
                }
                continue;
            }

            if (*sub != *topic) return false;

            sub++;
            topic++;
        }

        if (*sub == '#' && *(sub + 1) == '\0') return true;

        return (*sub == '\0' && *topic == '\0');
    }

    inline void _mqttRoute(char *topic, byte *payload, unsigned int length)
    {
        for (auto &sub : _eclMqttSubscriptions)
            if (_mqttTopicMatch(sub.topic.c_str(), topic))
                sub.callback(topic, payload, length);
    }

    inline void mqttPublish(const char *topic, const char *payload)
    {
        mqttClient.publish(topic, payload);
    }

    inline void mqttSubscribe(const char *topic, std::function<void(char *, byte *, unsigned int)> cb)
    {
        _eclMqttSubscriptions.push_back({topic, cb});
        if (mqttClient.connected())
            mqttClient.subscribe(topic);
    }

    inline void mqttSubscribe(const char *topic, std::function<void(char *, char *)> cb)
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

        if (mqttClient.connected())
            mqttClient.subscribe(topic);
    }

#endif

    // ========== INIT ==========
    inline void begin()
    {
#if defined(ECL_SERIAL_SPEED)
        Serial.begin(ECL_SERIAL_SPEED);
#endif

#if defined(ECL_WIFI_SSID)

        ECL::log.printf("Connecting to WiFi: %s\n", ECL_WIFI_SSID);

        WiFi.mode(WIFI_STA);
        WiFi.begin((const char*)ECL_WIFI_SSID, (const char*)ECL_WIFI_PASSWORD);

#if defined(ECL_OTA_HOSTNAME)
    #if defined(ESP32)
        WiFi.setHostname(ECL_OTA_HOSTNAME);
    #elif defined(ESP8266)
        WiFi.hostname(ECL_OTA_HOSTNAME);
    #endif
#endif

        while (WiFi.status() != WL_CONNECTED)
        {
            delay(500);
            Serial.print(".");
        }

        ECL::log.println("\nWiFi connected. IP: " + WiFi.localIP().toString());

#endif

#if defined(ECL_TELNET_PORT)
        telnetServer.begin();
#endif

#if defined(ECL_SOFTWARE_SERIAL_RX) && defined(ECL_SOFTWARE_SERIAL_TX)
        softwareSerial.begin(ECL_SOFTWARE_SERIAL_SPEED);
#endif

#if defined(ECL_MQTT_SERVER)
        mqttClient.setServer(ECL_MQTT_SERVER, ECL_MQTT_PORT);
        mqttClient.setCallback(_mqttRoute);
#endif

#if defined(ECL_OTA_HOSTNAME)
        ArduinoOTA.setHostname(ECL_OTA_HOSTNAME);
        ArduinoOTA.setPassword(ECL_OTA_PASSWORD);

        ArduinoOTA.onStart([](){ ECL::log.println("OTA Start"); });
        ArduinoOTA.onEnd([](){ ECL::log.println("\nOTA End"); });
        ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
        {
            ECL::log.printf("Progress: %u%%\r\n", (progress / (total / 100)));
        });
        ArduinoOTA.onError([](ota_error_t error)
        {
            ECL::log.printf("Error[%u]\n", error);
        });

        ArduinoOTA.begin();
        ECL::log.println("OTA Ready");
#endif
    }

    // ========== LOOP ==========
    inline void loop()
    {
#if defined(ECL_OTA_HOSTNAME)
        ArduinoOTA.handle();
#endif

#if defined(ECL_TELNET_PORT)

    #if defined(ESP32)
        if (telnetServer.hasClient())
            telnetClient = telnetServer.available();
    #elif defined(ESP8266)
        WiFiClient newClient = telnetServer.available();
        if (newClient)
            telnetClient = newClient;
    #endif

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

        for (auto &timer : _eclTimers)
        {
            if (currentMillis - timer.lastRun >= timer.interval)
            {
                timer.lastRun = currentMillis;
                timer.callback();
            }
        }
    }
}