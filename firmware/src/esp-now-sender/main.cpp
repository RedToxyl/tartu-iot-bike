#include <ESP8266WiFi.h>
#include <espnow.h>

// Replace with the MAC address of your receiver D1 Mini
uint8_t receiverMac[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  // Initialize ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  // Register peer
  esp_now_add_peer(receiverMac, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);
}

void loop() {
  uint8_t data = 0x01; // Ping payload
  esp_now_send(receiverMac, &data, sizeof(data));
  Serial.println("Ping sent");
  delay(1000);
}