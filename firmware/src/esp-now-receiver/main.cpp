#include <ESP8266WiFi.h>
#include <espnow.h>

void OnDataRecv(uint8_t *mac, uint8_t *data, uint8_t len) {
  Serial.print("Received: ");
  if (*data == 0x01) {
    Serial.println("Ping!");
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  // Initialize ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  // Register callback
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(OnDataRecv);
}

void loop() {
    Serial.println("My mac: " + WiFi.macAddress());
    delay(5000);
}