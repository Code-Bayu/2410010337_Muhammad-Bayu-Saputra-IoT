#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"

// ====== GANTI SESUAI KEBUTUHAN ======
const char* WIFI_SSID = "Wokwi-GUEST"; 
const char* WIFI_PASS = "";            
const char* MQTT_SERVER = "broker.emqx.io";
const int   MQTT_PORT = 1883;
const char* NIM = "2410010337";          
// ====================================

#define DHTPIN   4
#define DHTTYPE  DHT22
#define RELAY_PIN 26
#define LED_PIN   2
#define PWM_FREQ  5000
#define PWM_RES   8        // 8 bit -> 0-255

DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

String topicSuhu, topicRelay, topicPwm;
unsigned long lastSend = 0;
const unsigned long SEND_INTERVAL = 3000;  // 3 detik

void setPwm(int duty) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(LED_PIN, duty);
#else
  ledcWrite(0, duty);
#endif
}

void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  msg.trim();
  String t = String(topic);

  Serial.print("Pesan [");
  Serial.print(t);
  Serial.print("]: ");
  Serial.println(msg);

  if (t == topicRelay) {
    if (msg.equalsIgnoreCase("ON")) {
      digitalWrite(RELAY_PIN, HIGH);
      Serial.println("Relay ON");
    } else if (msg.equalsIgnoreCase("OFF")) {
      digitalWrite(RELAY_PIN, LOW);
      Serial.println("Relay OFF");
    }
  } else if (t == topicPwm) {
    int duty = constrain(msg.toInt(), 0, 255);
    setPwm(duty);
    Serial.print("PWM LED = ");
    Serial.println(duty);
  }
}

void setupWifi() {
  Serial.print("Menghubungkan ke WiFi ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi terhubung, IP: " + WiFi.localIP().toString());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Menghubungkan ke MQTT...");
    String clientId = "esp32-" + String(NIM) + "-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("terhubung");
      client.subscribe(topicRelay.c_str());
      client.subscribe(topicPwm.c_str());
    } else {
      Serial.print("gagal, rc=");
      Serial.print(client.state());
      Serial.println(" coba lagi 3 detik");
      delay(3000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  topicSuhu  = String("iot/") + 2410010337 + "/sensor/suhu";
  topicRelay = String("iot/") + 2410010337+ "/aktuator/relay";
  topicPwm   = String("iot/") + 2410010337 + "/aktuator/pwm";

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(LED_PIN, PWM_FREQ, PWM_RES);
#else
  ledcSetup(0, PWM_FREQ, PWM_RES);
  ledcAttachPin(LED_PIN, 0);
#endif
  setPwm(0);

  dht.begin();
  setupWifi();
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  unsigned long now = millis();
  if (now - lastSend >= SEND_INTERVAL) {
    lastSend = now;
    float suhu = dht.readTemperature();
    if (isnan(suhu)) {
      Serial.println("Gagal membaca DHT22");
      return;
    }
    char buf[10];
    dtostrf(suhu, 1, 1, buf);
    client.publish(topicSuhu.c_str(), buf);
    Serial.print("Suhu terkirim: ");
    Serial.println(buf);
  }
}
