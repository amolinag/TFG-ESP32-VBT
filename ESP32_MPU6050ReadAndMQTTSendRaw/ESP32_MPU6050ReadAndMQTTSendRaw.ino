#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <MPU6050.h>

const char* SSID     = "XXX";
const char* PASSWORD = "XXX";
const char* MQTT_BROKER  = "broker.hivemq.com";
const int   MQTT_PORT    = 1883;
const char* MQTT_TOPIC   = "vbt/amg/raw";
const char* CLIENT_ID    = "vbt-sensor-001";

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
MPU6050 mpu;

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
  Wire.begin(21, 22);
  Wire.setClock(100000);

  mpu.initialize();
  if (mpu.testConnection()) {
    Serial.println("MPU6050 OK");
  } else {
    Serial.println("MPU6050 ERROR");
    while(1);
  }

  conectarWifi();
  conectarMQTT();
}

void loop() {
  if (!mqttClient.connected()) conectarMQTT();
  mqttClient.loop();

  int16_t ax, ay, az, gx, gy, gz;
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

  // Convertir a m/s²
  float axMs = ax / 16384.0 * 9.81;
  float ayMs = ay / 16384.0 * 9.81;
  float azMs = az / 16384.0 * 9.81;

  // Magnitud total
  float magnitud = sqrt(axMs*axMs + ayMs*ayMs + azMs*azMs);

  char payload[128];
  snprintf(payload, sizeof(payload),
    "{\"ax\":%.2f,\"ay\":%.2f,\"az\":%.2f,\"mag\":%.2f}",
    axMs, ayMs, azMs, magnitud);

  mqttClient.publish(MQTT_TOPIC, payload);
  Serial.println(payload);

  delay(50); // 20Hz
}