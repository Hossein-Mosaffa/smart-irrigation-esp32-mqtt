// ESP32 irrigation node: reads soil moisture, drives a solenoid valve over MQTT.
// Topics (N = TREE_ID):  tree<N>/humidity (publish, %)   tree<N>/output (subscribe, "on"/"off")
// Libraries: PubSubClient (Nick O'Leary). Board package: esp32 by Espressif.
#include <WiFi.h>
#include <PubSubClient.h>
#include "secrets.h"   // WIFI_SSID, WIFI_PASS, MQTT_SERVER, TREE_ID, optional MQTT_USER/MQTT_PASS

const int VALVE_PIN  = 4;     // drives the transistor/relay circuit
const int SENSOR_PIN = 34;    // analog soil sensor (ADC1, input only)
const int ADC_DRY = 4095;     // raw value in dry air -> 0 %    (calibrate!)
const int ADC_WET = 0;        // raw value in water   -> 100 %  (calibrate!)
const unsigned long PUBLISH_MS = 5000;

WiFiClient espClient;
PubSubClient client(espClient);
char topicHum[32], topicOut[32], clientId[24];
unsigned long lastMsg = 0;

void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  msg.trim();
  Serial.printf("[%s] %s\n", topic, msg.c_str());
  if (msg == "on")       digitalWrite(VALVE_PIN, HIGH);
  else if (msg == "off") digitalWrite(VALVE_PIN, LOW);
}

void connectWifi() {
  Serial.printf("Connecting to %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.print("\nIP: "); Serial.println(WiFi.localIP());
}

void connectMqtt() {
  while (!client.connected()) {
    Serial.print("MQTT... ");
#ifdef MQTT_USER
    bool ok = client.connect(clientId, MQTT_USER, MQTT_PASS);
#else
    bool ok = client.connect(clientId);
#endif
    if (ok) {
      client.subscribe(topicOut);
      Serial.println("connected");
    } else {
      Serial.printf("failed rc=%d, retry in 5 s\n", client.state());
      delay(5000);
    }
  }
}

float readMoisture() {
  long sum = 0;
  for (int i = 0; i < 16; i++) { sum += analogRead(SENSOR_PIN); delay(2); }
  float raw = sum / 16.0;
  Serial.printf("raw ADC = %.0f\n", raw);   // use this to calibrate
  float pct = (raw - ADC_DRY) * 100.0 / (ADC_WET - ADC_DRY);
  return constrain(pct, 0.0, 100.0);
}

void setup() {
  Serial.begin(115200);
  pinMode(VALVE_PIN, OUTPUT);
  digitalWrite(VALVE_PIN, LOW);            // valve closed at boot
  snprintf(topicHum, sizeof(topicHum), "tree%d/humidity", TREE_ID);
  snprintf(topicOut, sizeof(topicOut), "tree%d/output", TREE_ID);
  snprintf(clientId, sizeof(clientId), "Esp%dClient", TREE_ID);
  connectWifi();
  client.setServer(MQTT_SERVER, 1883);
  client.setCallback(callback);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWifi();
  if (!client.connected()) connectMqtt();
  client.loop();

  if (millis() - lastMsg >= PUBLISH_MS) {
    lastMsg = millis();
    char buf[8];
    dtostrf(readMoisture(), 1, 1, buf);
    client.publish(topicHum, buf);
    Serial.printf("%s = %s\n", topicHum, buf);
  }
}
