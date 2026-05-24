#define ECL_HARDWARE_SERIAL_PORT 2
#define ECL_HARDWARE_SERIAL_RX 17
#define ECL_HARDWARE_SERIAL_TX 16
#define ECL_HARDWARE_SERIAL_SPEED 9600

#include "ECL.h"

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <nvs_flash.h>

#define DEVICE_NAME "ESP_MESH_BRIDGE"
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

NimBLEServer *pServer = nullptr;
NimBLECharacteristic *pCharacteristic = nullptr;

class ServerCallbacks : public NimBLEServerCallbacks
{
    void onConnect(NimBLEServer *pServer)
    {
        ECL::log.printf("Client Connected! Total: %d\n", pServer->getConnectedCount());
        NimBLEDevice::startAdvertising();
    }
    void onDisconnect(NimBLEServer *pServer)
    {
        ECL::log.printf("Client Disconnected! Total: %d\n", pServer->getConnectedCount());
        NimBLEDevice::startAdvertising();
    }
};

class CharCallbacks : public NimBLECharacteristicCallbacks
{
    void onWrite(NimBLECharacteristic *pChar)
    {
        std::string rxValue = pChar->getValue();
        if (rxValue.length() > 0)
        {
            hardwareSerial.print(rxValue.c_str());
            ECL::log.print("To Master: ");
            ECL::log.print(rxValue.c_str());
        }
    }
};

void setup()
{
    ECL::begin();

    nvs_flash_erase();
    nvs_flash_init();

    NimBLEDevice::init(DEVICE_NAME);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    NimBLEService *pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        NIMBLE_PROPERTY::READ |
            NIMBLE_PROPERTY::WRITE |
            NIMBLE_PROPERTY::NOTIFY);
    pCharacteristic->setCallbacks(new CharCallbacks());
    pService->start();

    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setName(DEVICE_NAME);
    pAdvertising->enableScanResponse(true);
    pAdvertising->setMinInterval(32);
    pAdvertising->setMaxInterval(64);
    pAdvertising->start();

    Serial.println("Bridge Ready. Waiting for clients...");
}

void loop()
{
    ECL::loop();
    if (hardwareSerial.available())
    {
        String incoming = hardwareSerial.readStringUntil('\n');
        incoming.trim();
        if (incoming.length() > 0)
        {
            incoming += "\n";
            if (pServer->getConnectedCount() > 0)
            {
                pCharacteristic->setValue(incoming.c_str());
                pCharacteristic->notify();
                ECL::log.print("Forwarded: ");
                ECL::log.print(incoming);
            }
            else
            {
                ECL::log.print("Dropped (No Client): ");
                ECL::log.print(incoming);
            }
        }
    }
}