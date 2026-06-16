/*
 * SupplyMesh Edge - ESP32 IoT Device Simulator
 * Device: SME001
 * Author: Kaushal Kumar
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>

#define WIFI_SSID    "Wokwi-GUEST"
#define WIFI_PASS    ""

#define MQTT_BROKER  "broker.hivemq.com"
#define MQTT_PORT    1883
#define MQTT_TOPIC   "supplymesh/SME001/telemetry"
#define MQTT_CLIENT  "SME001-ESP32"

#define DEVICE_ID    "SME001"
#define PUBLISH_MS   10000UL

// Pins
#define DHT_PIN     15
#define DHT_TYPE    DHT22
#define OLED_SDA    21
#define OLED_SCL    22
#define RGB_R       25
#define RGB_G       26
#define RGB_B       27
#define BTN_PIN     14

#define SCREEN_W    128
#define SCREEN_H    64
#define OLED_ADDR   0x3C

// GPS start position (New Delhi)
#define GPS_LAT_INIT   28.61f
#define GPS_LON_INIT   77.20f

#define BATT_INIT   95.0f
#define BATT_DRAIN  0.05f   // percent per publish cycle

WiFiClient        wifiClient;
PubSubClient      mqtt(wifiClient);
DHT               dht(DHT_PIN, DHT_TYPE);
Adafruit_SSD1306  oled(SCREEN_W, SCREEN_H, &Wire, -1);

float    lat         = GPS_LAT_INIT;
float    lon         = GPS_LON_INIT;
float    battery     = BATT_INIT;
int      pubCount    = 0;
float    lastTemp    = 25.0f;
float    lastHum     = 70.0f;

unsigned long  lastPublish   = 0;
unsigned long  lastOLEDtick  = 0;
volatile bool  btnFlag       = false;

// Common cathode: HIGH = ON
// Yellow=WiFi, Blue=MQTT, Green=ready, Red=error, White=publishing
void setRGB(bool r, bool g, bool b) {
  digitalWrite(RGB_R, r ? HIGH : LOW);
  digitalWrite(RGB_G, g ? HIGH : LOW);
  digitalWrite(RGB_B, b ? HIGH : LOW);
}

void flashPublish() {
  setRGB(true, true, true);
  delay(150);
  setRGB(false, true, false);
}

// ISR sets flag only - actual publish runs in loop to avoid blocking
void IRAM_ATTR onBtn() {
  btnFlag = true;
}

void splashScreen() {
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(12, 14);
  oled.println("SupplyMesh Edge(TM)");
  oled.setCursor(36, 28);
  oled.println("SME001  v1.0");
  oled.setCursor(22, 44);
  oled.println("Initialising...");
  oled.display();
  delay(1500);
}

void connectWiFi() {
  setRGB(true, true, false);

  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(4, 20);
  oled.println("Connecting WiFi...");
  oled.setCursor(4, 32);
  oled.println(WIFI_SSID);
  oled.display();

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
  }

  oled.clearDisplay();
  oled.setCursor(4, 22);
  oled.println("WiFi connected");
  oled.setCursor(4, 34);
  oled.println(WiFi.localIP().toString());
  oled.display();
  delay(1200);

  Serial.print("[WiFi] IP: ");
  Serial.println(WiFi.localIP());
}

void connectMQTT() {
  setRGB(false, false, true);

  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setBufferSize(300);

  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(4, 16);
  oled.println("Connecting MQTT...");
  oled.setCursor(4, 28);
  oled.println(MQTT_BROKER);
  oled.display();

  int attempt = 0;
  while (!mqtt.connected()) {
    attempt++;
    if (mqtt.connect(MQTT_CLIENT)) {
      break;
    }
    oled.clearDisplay();
    oled.setCursor(4, 22);
    oled.print("MQTT retry #");
    oled.println(attempt);
    oled.display();
    Serial.print("[MQTT] Retry #");
    Serial.println(attempt);
    delay(2000);
  }

  oled.clearDisplay();
  oled.setCursor(4, 16);
  oled.println("MQTT connected");
  oled.setCursor(4, 30);
  oled.println("Topic:");
  oled.setCursor(4, 42);
  oled.println("supplymesh/");
  oled.setCursor(4, 52);
  oled.println("SME001/telemetry");
  oled.display();
  delay(1500);

  setRGB(false, true, false);
  Serial.println("[MQTT] Connected");
}

// random(-100, 101) / 10000 gives +-0.01 degrees drift per cycle
void walkGPS() {
  float dLat = (float)(random(-100, 101)) / 10000.0f;
  float dLon = (float)(random(-100, 101)) / 10000.0f;
  lat = constrain(lat + dLat, 28.50f, 28.72f);
  lon = constrain(lon + dLon, 77.10f, 77.30f);
}

// OLED layout (128x64, textSize=1):
// y=0  device_id + MQTT status | y=11 temp | y=20 hum | y=29 battery
// y=38 latitude | y=47 longitude | y=56 publish count + countdown
void updateOLED(float temp, float hum, unsigned long elapsed) {
  int secLeft = (int)((PUBLISH_MS - elapsed) / 1000);
  if (secLeft < 0) secLeft = 0;

  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);

  oled.setCursor(0, 0);
  oled.print(DEVICE_ID);
  oled.setCursor(72, 0);
  oled.print(mqtt.connected() ? "MQTT:OK" : "MQTT:--");

  oled.drawLine(0, 9, 127, 9, SSD1306_WHITE);

  oled.setCursor(0, 11);
  oled.print("Temp : ");
  oled.print(temp, 1);
  oled.print(" C");

  oled.setCursor(0, 20);
  oled.print("Hum  : ");
  oled.print((int)round(hum));
  oled.print(" %");

  oled.setCursor(0, 29);
  oled.print("Batt : ");
  oled.print((int)round(battery));
  oled.print(" %");

  oled.setCursor(0, 38);
  oled.print("Lat  : ");
  oled.print(lat, 4);

  oled.setCursor(0, 47);
  oled.print("Lon  : ");
  oled.print(lon, 4);

  oled.setCursor(0, 56);
  oled.print("Pub#");
  oled.print(pubCount);
  oled.setCursor(90, 56);
  oled.print("T-");
  oled.print(secLeft);
  oled.print("s");

  oled.display();
}

void publishData() {
  float temp = dht.readTemperature();
  float hum  = dht.readHumidity();

  // fall back to last valid reading if sensor returns NaN
  if (isnan(temp) || isnan(hum)) {
    temp = lastTemp;
    hum  = lastHum;
  } else {
    lastTemp = temp;
    lastHum  = hum;
  }

  walkGPS();
  battery = max(0.0f, battery - BATT_DRAIN);
  pubCount++;

  // dtostrf gives exact decimal places without float rounding issues
  char tempBuf[10], latBuf[12], lonBuf[12];
  dtostrf(temp, 0, 1, tempBuf);
  dtostrf(lat,  0, 4, latBuf);
  dtostrf(lon,  0, 4, lonBuf);

  StaticJsonDocument<300> doc;
  doc["device_id"]   = DEVICE_ID;
  doc["temperature"] = serialized(tempBuf);
  doc["humidity"]    = (int)round(hum);
  doc["battery"]     = (int)round(battery);
  doc["latitude"]    = serialized(latBuf);
  doc["longitude"]   = serialized(lonBuf);

  char payload[256];
  serializeJson(doc, payload, sizeof(payload));

  if (mqtt.connected()) {
    mqtt.publish(MQTT_TOPIC, payload, true);
    flashPublish();
  } else {
    setRGB(true, false, false);
  }

  Serial.println("\n------------------------------------------");
  Serial.print("[Pub #"); Serial.print(pubCount); Serial.println("]");
  char pretty[350];
  serializeJsonPretty(doc, pretty, sizeof(pretty));
  Serial.println(pretty);
  Serial.print("[Topic] "); Serial.println(MQTT_TOPIC);

  lastPublish = millis();
  updateOLED(temp, hum, 0);
}

void ensureConnected() {
  if (WiFi.status() != WL_CONNECTED) {
    setRGB(true, true, false);
    connectWiFi();
  }
  if (!mqtt.connected()) {
    setRGB(false, false, true);
    connectMQTT();
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(RGB_R, OUTPUT);
  pinMode(RGB_G, OUTPUT);
  pinMode(RGB_B, OUTPUT);
  setRGB(false, false, false);

  pinMode(BTN_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BTN_PIN), onBtn, FALLING);

  randomSeed(esp_random());

  Wire.begin(OLED_SDA, OLED_SCL);
  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("[OLED] init failed");
    while (true);
  }
  oled.clearDisplay();
  oled.display();

  splashScreen();
  dht.begin();
  connectWiFi();
  connectMQTT();
  publishData();
}

void loop() {
  mqtt.loop();
  ensureConnected();

  unsigned long now     = millis();
  unsigned long elapsed = now - lastPublish;

  if (btnFlag) {
    btnFlag = false;
    delay(50);
    publishData();
    return;
  }

  if (elapsed >= PUBLISH_MS) {
    publishData();
    return;
  }

  if (now - lastOLEDtick >= 1000) {
    updateOLED(lastTemp, lastHum, elapsed);
    lastOLEDtick = now;
  }
}
