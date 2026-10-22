#include <WiFi.h>
#include <PubSubClient.h>

const char* SSID     = "XXX";
const char* PASSWORD = "XXX";
const char* MQTT_BROKER = "broker.hivemq.com";
const int   MQTT_PORT   = 1883;
const char* MQTT_TOPIC  = "vbt/amg/test";
const char* CLIENT_ID   = "vbt-sensor-001";

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

void conectarWifi() {
  Serial.print("Conectando WiFi");
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi OK: " + WiFi.localIP().toString());
}

void conectarMQTT() {
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  while (!mqttClient.connected()) {
    Serial.print("Conectando MQTT...");
    if (mqttClient.connect(CLIENT_ID)) {
      Serial.println("OK");
    } else {
      Serial.println("fallo, reintentando...");
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  conectarWifi();
  conectarMQTT();
}

void loop() {
  if (!mqttClient.connected()) conectarMQTT();
  mqttClient.loop();

  mqttClient.publish(MQTT_TOPIC, "hola desde ESP32");
  Serial.println("Mensaje enviado");
  delay(2000);
}