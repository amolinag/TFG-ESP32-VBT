#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <MPU6050.h>

const char* SSID     = "XXX";
const char* PASSWORD = "XXX";
const char* MQTT_BROKER = "broker.hivemq.com";
const int   MQTT_PORT   = 1883;
const char* TOPIC_RAW   = "vbt/amg/raw";
const char* TOPIC_REP   = "vbt/amg/rep";
const char* TOPIC_SERIE = "vbt/amg/serie";
const char* CLIENT_ID   = "vbt-sensor-001";

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
MPU6050 mpu;

// ── Umbrales ──
const float UMBRAL_INICIO    = 10.8;  // m/s² — magnitud para detectar inicio de rep
const float UMBRAL_FIN       = 10.2;  // m/s² — magnitud para detectar fin de rep
const int   MUESTRAS_FIN     = 10;    // muestras consecutivas bajo umbral para confirmar fin
const unsigned long TIMEOUT_SERIE = 10000; // 10s sin movimiento = fin de serie

// ── Estado VBT ──
bool enRepeticion      = false;
float velocidad        = 0.0;
float velocidadPico    = 0.0;
float sumaVelocidades  = 0.0;
int   lecturas         = 0;
int   muestrasBajas    = 0;
int   numRep           = 0;
int   numSerie         = 1;

// Para fin de serie
unsigned long ultimoMovimiento = 0;
bool serieActiva = false;

unsigned long tiempoAnterior = 0;

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

void publicarRep(float vMedia, float vPico) {
  char payload[128];
  snprintf(payload, sizeof(payload),
    "{\"serie\":%d,\"rep\":%d,\"v_media\":%.2f,\"v_pico\":%.2f}",
    numSerie, numRep, vMedia, vPico);
  mqttClient.publish(TOPIC_REP, payload);
  Serial.println("REP: " + String(payload));
}

void publicarSerie(int reps) {
  char payload[64];
  snprintf(payload, sizeof(payload),
    "{\"serie\":%d,\"reps_totales\":%d}",
    numSerie, reps);
  mqttClient.publish(TOPIC_SERIE, payload);
  Serial.println("SERIE: " + String(payload));
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

  tiempoAnterior = millis();
  ultimoMovimiento = millis();
}

void loop() {
  if (!mqttClient.connected()) conectarMQTT();
  mqttClient.loop();

  unsigned long ahora = millis();
  float dt = (ahora - tiempoAnterior) / 1000.0;
  tiempoAnterior = ahora;

  // ── Lectura sensor ──
  int16_t ax, ay, az, gx, gy, gz;
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

  float axMs = ax / 16384.0 * 9.81;
  float ayMs = ay / 16384.0 * 9.81;
  float azMs = az / 16384.0 * 9.81;
  float mag  = sqrt(axMs*axMs + ayMs*ayMs + azMs*azMs);

  // Publicar raw
  char rawPayload[128];
  snprintf(rawPayload, sizeof(rawPayload),
    "{\"ax\":%.2f,\"ay\":%.2f,\"az\":%.2f,\"mag\":%.2f}",
    axMs, ayMs, azMs, mag);
  mqttClient.publish(TOPIC_RAW, rawPayload);

  // ── Lógica VBT ──
  float aMovimiento = mag - 9.81;

  if (!enRepeticion) {
    // Detectar inicio de rep
    if (mag > UMBRAL_INICIO) {
      enRepeticion    = true;
      serieActiva     = true;
      velocidad       = 0.0;
      velocidadPico   = 0.0;
      sumaVelocidades = 0.0;
      lecturas        = 0;
      muestrasBajas   = 0;
      numRep++;
      ultimoMovimiento = ahora;
      Serial.println(">>> Rep " + String(numRep) + " iniciada");
    }

    // Detectar fin de serie por timeout
    if (serieActiva && (ahora - ultimoMovimiento > TIMEOUT_SERIE)) {
      publicarSerie(numRep);
      serieActiva = false;
      numRep      = 0;
      numSerie++;
    }

  } else {
    // Integración velocidad
    if (aMovimiento > 0) {
      velocidad += aMovimiento * dt;
    } else {
      velocidad *= 0.95; // decay suave cuando decelera
    }

    if (velocidad < 0) velocidad = 0;
    if (velocidad > velocidadPico) velocidadPico = velocidad;

    sumaVelocidades += velocidad;
    lecturas++;
    ultimoMovimiento = ahora;

    // Detectar fin de rep
    if (mag < UMBRAL_FIN) {
      muestrasBajas++;
      if (muestrasBajas >= MUESTRAS_FIN) {
        enRepeticion = false;
        float vMedia = lecturas > 0 ? sumaVelocidades / lecturas : 0;
        publicarRep(vMedia, velocidadPico);
      }
    } else {
      muestrasBajas = 0;
    }
  }

  delay(50); // 20Hz
}