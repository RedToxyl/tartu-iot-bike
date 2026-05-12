#include "ECL.h"
#include <HTTPClient.h>

HTTPClient http;

void request()
{
    http.begin("https://httpbin.org/get");

    int httpCode = http.GET();

    if (httpCode > 0)
    {
        ECL::log.printf("HTTP Response code: %d\n", httpCode);

        String payload = http.getString();

        ECL::log.println(payload);
    }
    else
    {
        ECL::log.printf("HTTP GET failed: %s\n",
                        http.errorToString(httpCode).c_str());
    }

    http.end();
}

void setup()
{
    ECL::begin(); // initializes OTA, MQTT, Telnet
    delay (2000); // wait for WiFi to connect
    ECL::log.printf("My mac adress is [%s]\n", WiFi.macAddress().c_str()); // printf via Serial & Telnet
}

void loop()
{
    ECL::loop();                                                           // handles OTA, MQTT, Telnet Buttons etc
    
    request();                                                                // example HTTP request to test WiFi connectivity

    delay(1000);
}