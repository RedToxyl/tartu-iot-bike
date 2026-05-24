#define ECL_ESPNOW_ENABLE

#include "ECL.h"
#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN  5
#define RST_PIN 27

#ifndef SPACE_ID
#define SPACE_ID 0
#endif

MFRC522 mfrc522(SS_PIN, RST_PIN);

void setup()
{
    SPI.begin(18, 19, 23, SS_PIN); // SCK, MISO, MOSI, SS

    ECL::begin(); // WiFi + ESP-NOW init first
    mfrc522.PCD_Init(); // init RC522 after WiFi settles to prevent SPI interference
    ECL::log.println("RFID reader ready");
}

void loop()
{
    ECL::loop();

    if (!mfrc522.PICC_IsNewCardPresent()) return;
    if (!mfrc522.PICC_ReadCardSerial()) return;

    // Build UID string
    String uidStr = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
        if (mfrc522.uid.uidByte[i] < 0x10) uidStr += "0";
        uidStr += String(mfrc522.uid.uidByte[i], HEX);
    }
    uidStr.toUpperCase();

    ECL::log.print("Card UID: ");
    ECL::log.println(uidStr);

    String topic = "space/" + String(SPACE_ID) + "/rfid";
    ECL::publish(topic.c_str(), uidStr.c_str());

    mfrc522.PICC_HaltA();
    delay(500);
}
