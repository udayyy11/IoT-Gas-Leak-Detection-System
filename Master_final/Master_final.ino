/**************** MASTER NODE – FINAL STABLE ****************/

#define BLYNK_TEMPLATE_ID   "TMPL3dj-RlZdw"
#define BLYNK_TEMPLATE_NAME "sensor node 1"
#define BLYNK_AUTH_TOKEN    "X1M90Tz_c7PO_2ZuLlnraV1UDR_No2Ox"

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <BlynkSimpleEsp32.h>
#include <ThingSpeak.h>

/* ---------------- WIFI ---------------- */
char ssid[] = "YOUR_SSID";
char pass[] = "YOUR_PASSWORD";

/* ---------------- THINGSPEAK ---------------- */
unsigned long channelID   = 3197735;
const char*  ";
WiFiClient    client; writeAPIKey = "BAOKOG4RAL9YHYO5

/* ---------------- BLYNK TIMER ---------------- */
BlynkTimer timer;

/* ---------------- ACTUATORS ---------------- */
#define RED_LED   26
#define GREEN_LED 27
#define BUZZER    25

/* ---------------- NODE MANAGEMENT ---------------- */
#define MAX_NODES    3
#define NODE_TIMEOUT 10000UL

unsigned long lastSeen[MAX_NODES]  = {0};
bool          nodeActive[MAX_NODES]= {false};

/* ---------------- DATA STRUCT ---------------- */
typedef struct {
  uint8_t  node_id;
  uint16_t airQuality;
  float    temperature;
  float    humidity;
  float    nh3;
  float    nox;
  float    co2;
} SensorData;

typedef struct {
  uint8_t node_id;
  bool    ack;
} AckPacket;

SensorData node[MAX_NODES];

/* ---------------- GLOBAL VALUES ---------------- */
uint16_t maxAQ = 0;
float avgTemp = 0, avgHum = 0;
float avgNH3 = 0, avgNOX = 0, avgCO2 = 0;
float leakProb = 0;
int leakNode = 0;
int activeCount = 0;
String systemState = "SAFE";
bool danger = false;

/* ---------------- BLYNK CONTROL ---------------- */
bool manualRed = false;
bool manualBuzzer = false;
BLYNK_WRITE(V8) { manualRed = param.asInt(); }
BLYNK_WRITE(V9) { manualBuzzer = param.asInt(); }

/* ================================================================ */
void pushToBlynk() {
  if (!Blynk.connected()) return;

  Blynk.virtualWrite(V0, maxAQ);
  Blynk.virtualWrite(V1, avgTemp);
  Blynk.virtualWrite(V2, avgNH3);
  Blynk.virtualWrite(V3, avgNOX);
  Blynk.virtualWrite(V4, avgCO2);
  Blynk.virtualWrite(V7, avgHum);
  Blynk.virtualWrite(V10, leakProb);
  Blynk.virtualWrite(V11, leakNode);
}

/* ================================================================ */
void pushToThingSpeak() {
  ThingSpeak.setField(1, (int)maxAQ);
  ThingSpeak.setField(2, avgTemp);
  ThingSpeak.setField(3, avgNH3);
  ThingSpeak.setField(4, avgNOX);
  ThingSpeak.setField(5, avgCO2);
  ThingSpeak.setField(6, activeCount);
  ThingSpeak.setField(7, leakNode);
  ThingSpeak.setField(8, avgHum);

  int status = ThingSpeak.writeFields(channelID, writeAPIKey);
  Serial.printf("☁️ ThingSpeak: %s (%d)\n",
                status == 200 ? "OK" : "FAIL", status);
}

/* ================================================================ */
void aggregateData() {

  for (int i = 0; i < MAX_NODES; i++) {
    if (nodeActive[i] && millis() - lastSeen[i] > NODE_TIMEOUT) {
      nodeActive[i] = false;
    }
  }

  activeCount = 0;
  for (int i = 0; i < MAX_NODES; i++)
    if (nodeActive[i]) activeCount++;

  if (activeCount == 0) return;

  maxAQ = 0;
  float sumTemp=0, sumHum=0, sumNH3=0, sumNOX=0, sumCO2=0;

  for (int i = 0; i < MAX_NODES; i++) {
    if (!nodeActive[i]) continue;

    if (node[i].airQuality > maxAQ) {
      maxAQ = node[i].airQuality;
      leakNode = i + 1;
    }

    sumTemp += node[i].temperature;
    sumHum  += node[i].humidity;
    sumNH3  += node[i].nh3;
    sumNOX  += node[i].nox;
    sumCO2  += node[i].co2;
  }

  avgTemp = sumTemp / activeCount;
  avgHum  = sumHum  / activeCount;
  avgNH3  = sumNH3  / activeCount;
  avgNOX  = sumNOX  / activeCount;
  avgCO2  = sumCO2  / activeCount;

  leakProb = (maxAQ / 500.0f) * 60 + (avgTemp / 50.0f) * 20;
  leakProb = constrain(leakProb, 0, 100);

  systemState = (leakProb < 30) ? "SAFE" :
                (leakProb < 70) ? "WARNING" : "DANGER";

  danger = (systemState == "DANGER");

  digitalWrite(RED_LED, manualRed || danger);
  digitalWrite(GREEN_LED, !(manualRed || danger));
}

/* ================================================================ */
void onReceive(const esp_now_recv_info_t* info,
               const uint8_t* data, int len) {

  if (len != sizeof(SensorData)) return;

  SensorData incoming;
  memcpy(&incoming, data, sizeof(incoming));

  int idx = incoming.node_id - 1;

  node[idx] = incoming;
  nodeActive[idx] = true;
  lastSeen[idx] = millis();

  AckPacket ack = { incoming.node_id, true };
  esp_now_send(info->src_addr, (uint8_t*)&ack, sizeof(ack));

  Serial.printf("📡 Node %d received\n", incoming.node_id);
}

/* ================================================================ */
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  digitalWrite(GREEN_LED, HIGH);

  /* ---- WiFi ---- */
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  Serial.print("Connecting...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n✅ WiFi Connected");
  Serial.println(WiFi.localIP());

  // ✅ CRITICAL FIXES
  esp_wifi_set_ps(WIFI_PS_NONE);

  int channel = WiFi.channel();
  Serial.print("📡 Channel: ");
  Serial.println(channel);

  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);

  Serial.print("📱 MAC: ");
  Serial.println(WiFi.macAddress());

  /* ---- Blynk ---- */
  Blynk.config(BLYNK_AUTH_TOKEN);
  Blynk.connect();

  /* ---- ThingSpeak ---- */
  ThingSpeak.begin(client);

  /* ---- ESP-NOW ---- */
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW Failed");
    ESP.restart();
  }

  esp_now_register_recv_cb(onReceive);

  /* ---- Timers ---- */
  timer.setInterval(2000L, aggregateData);
  timer.setInterval(2000L, pushToBlynk);
  timer.setInterval(20000L, pushToThingSpeak);

  Serial.println("🚀 MASTER READY");
}

/* ================================================================ */
bool buzzerOn = false;
unsigned long buzzerTime = 0;

void loop() {
  Blynk.run();
  timer.run();

  if ((manualBuzzer || danger) && !buzzerOn) {
    digitalWrite(BUZZER, HIGH);
    buzzerOn = true;
    buzzerTime = millis();
  }

  if (buzzerOn && millis() - buzzerTime > 500) {
    digitalWrite(BUZZER, LOW);
    buzzerOn = false;
  }
}