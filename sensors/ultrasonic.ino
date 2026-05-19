//WemosD1mini

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// ===== WIFI SETTINGS =====
const char* ssid = "WIN-J3A5";
const char* password = "904,V84q";

// ===== MQTT SETTINGS =====
const char* mqtt_server = "192.168.137.1";
const int mqtt_port = 1883;
const char* mqtt_topic = "sensor/ultrasonic"; // MQTT topic to publish distance

// ===== ULTRASONIC SENSOR PINS =====
#define TRIG_PIN D1
#define ECHO_PIN D2

WiFiClient espClient;
PubSubClient client(espClient);

// ===== FUNCTIONS =====
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Wi-Fi connected. IP: ");
  Serial.println(WiFi.localIP());
}

void reconnect_mqtt() {
  // Loop until connected
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect("ESP32_Ultrasonic")) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

long measureDistanceCM() {
  // Send 10µs pulse to TRIG
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Read pulse width from ECHO
  long duration = pulseIn(ECHO_PIN, HIGH, 50000); // max 50ms timeout
  if (duration == 0) {
    Serial.println("No echo detected!");
    return 0;
  }

  long distance_cm = duration * 0.034 / 2;
  return distance_cm;
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  Serial.println("Ultrasonic sensor with MQTT starting...");

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
}

void loop() {
  if (!client.connected()) {
    reconnect_mqtt();
  }
  client.loop();

  long distance = measureDistanceCM();
  if (distance > 0) {
    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.println(" cm");

    // Publish distance to MQTT
    char msg[16];
    snprintf(msg, sizeof(msg), "%ld", distance);
    client.publish(mqtt_topic, msg);
  }

  delay(1000); // Measure and publish every second
}