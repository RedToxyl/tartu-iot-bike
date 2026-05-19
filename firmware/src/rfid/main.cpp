#define ECL_ESPNOW_ENABLE

#include "ECL.h"
#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN  5
#define RST_PIN 27

MFRC522 mfrc522(SS_PIN, RST_PIN);

void setup()
{
    SPI.begin(18, 19, 23, SS_PIN); // SCK, MISO, MOSI, SS
    mfrc522.PCD_Init();

    ECL::begin();
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

    ECL::publish("sensor/rfid", uidStr.c_str());

    mfrc522.PICC_HaltA();
    delay(500);
}
