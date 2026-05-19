//ESP32 Minikit

#include <WiFi.h>
#include <SPI.h>
#include <MFRC522.h>
#include <PubSubClient.h>

// ===== WIFI SETTINGS =====
const char* ssid = "WIN-J3A5";
const char* password = "904,V84q";

// ===== MQTT SETTINGS =====
const char* mqtt_server = "192.168.137.1";
const int mqtt_port = 1883;
const char* mqtt_topic = "sensor/rfid";

// ===== RC522 PINS (ESP32 SAFE PINS) =====
#define SS_PIN   5
#define RST_PIN  27

MFRC522 mfrc522(SS_PIN, RST_PIN);

// ===== MQTT CLIENT =====
WiFiClient espClient;
PubSubClient client(espClient);

// ===== WIFI SETUP =====
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

// ===== MQTT RECONNECT =====
void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting MQTT...");

    if (client.connect("ESP32_RC522")) {
      Serial.println("connected");
    } else {
      Serial.print("failed rc=");
      Serial.print(client.state());
      Serial.println(" retry in 5s");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  // SPI for ESP32 (important!)
  SPI.begin(18, 19, 23, SS_PIN); // SCK, MISO, MOSI, SS

  mfrc522.PCD_Init();
  Serial.println("RC522 ready");

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial()) return;

  // Build UID string
  String uidStr = "";

  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) uidStr += "0";
    uidStr += String(mfrc522.uid.uidByte[i], HEX);
  }

  uidStr.toUpperCase();

  Serial.print("Card UID: ");
  Serial.println(uidStr);

  client.publish(mqtt_topic, uidStr.c_str());

  mfrc522.PICC_HaltA();
  delay(500);
}