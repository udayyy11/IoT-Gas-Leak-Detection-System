#include <WiFi.h>
#include <esp_now.h>
#include <DHT.h>
#include <math.h>

#define NODE_ID 2     //  sensor node 1

#define DHTPIN 4
#define DHTTYPE DHT11
#define MQ135_PIN 34

DHT dht(DHTPIN, DHTTYPE);

/* -------- MASTER NODE MAC ADDRESS -------- */
uint8_t MASTER_MAC[] = {0xB0, 0xCB, 0xD8, 0xE2, 0xA0, 0xD4};

/* -------- MQ-135 -------- */
float RL = 10.0;
float R0 = 10.0;

/* -------- DATA STRUCT -------- */
typedef struct {
  int node_id;
  int airQuality;
  float temperature;
  float humidity;
  float nh3;
  float nox;
  float co2;
  int alcohol;
  int benzene;
} SensorData;

SensorData data;

void setup() {
  Serial.begin(115200);
  dht.begin();

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, MASTER_MAC, 6);
  peer.channel = 0;
  peer.encrypt = false;
  esp_now_add_peer(&peer);

  Serial.print("Sensor Node ");
  Serial.print(NODE_ID);
  Serial.println(" Ready");
}

void loop() {

  int rawADC = analogRead(MQ135_PIN);
  if (rawADC < 10) rawADC = 10;

  data.node_id = NODE_ID;
  data.airQuality = rawADC / 9;
  data.temperature = dht.readTemperature();
  data.humidity = dht.readHumidity();

  float Rs = RL * (4095.0 - rawADC) / rawADC;
  float ratio = Rs / R0;

  data.nh3 = 102.2 * pow(ratio, -2.473);
  data.nox = 100.0 * pow(ratio, -1.5);
  data.co2 = 116.6 * pow(ratio, -2.769);

  data.alcohol = (ratio < 1.0);
  data.benzene = (ratio < 0.8);

  esp_now_send(MASTER_MAC, (uint8_t*)&data, sizeof(data));

  Serial.print("Data sent from Node ");
  Serial.println(NODE_ID);

  delay(3000);
}
