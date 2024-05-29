#include <DHT.h>
#include <WiFi.h>
#include <PubSubClient.h>

#define DHTPIN 14
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

int ldrPin = 32;
int ledPin = 19;
const float GAMMA = 0.7;
const float RL10 = 50;

#define LED_PIN 26

const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "0.tcp.ap.ngrok.io"; // Replace with Ngrok provided forwarding address
const int mqtt_port = 16213; // Replace with the port provided by Ngrok

WiFiClient espClient;
PubSubClient client(espClient);

// Variables for lamp control timing
unsigned long lampOnTime = 0;
unsigned long lampOffTime = 0;
const unsigned long lampDuration = 5000; // Lamp stays on/off for 5 seconds

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

String getUniqueClientId() {
  uint64_t chipId = ESP.getEfuseMac(); // The chip ID is essentially the MAC address
  String clientId = "ESP32Client-";
  clientId += String((uint16_t)(chipId >> 32), HEX);
  clientId += String((uint32_t)chipId, HEX);
  return clientId;
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);

  // Handle the message
  if (String(topic) == "plant/photoresistor/lamp") {
    if (message == "ON") {
      digitalWrite(ledPin, HIGH);
      Serial.println("Lampu: ON");
      lampOnTime = millis();
    } else if (message == "OFF") {
      digitalWrite(ledPin, LOW);
      Serial.println("Lampu: OFF");
      lampOffTime = millis();
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = getUniqueClientId();
    
    // Define LWT message
    const char* willTopic = "plant/status";
    const char* willMessage = "ESP32 disconnected";
    const int willQos = 0;
    const bool willRetain = false;
    
    if (client.connect(clientId.c_str(), willTopic, willQos, willRetain, willMessage)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("plant/status", "ESP32 connected");
      // ... and resubscribe
      client.subscribe("plant/photoresistor/lamp");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(9600);
  dht.begin();
  pinMode(LED_PIN, OUTPUT);
  pinMode(ledPin, OUTPUT);

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  client.setKeepAlive(15);

}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  float h = dht.readHumidity();
  float t = dht.readTemperature();
  float hic = dht.computeHeatIndex(t, h, false);

  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.print(F("% Temperature: "));
  Serial.print(t);
  Serial.print(" *C ");

  if (t > 30) {
    digitalWrite(LED_PIN, HIGH);
    Serial.println("LED 1: ON");
  } else {
    digitalWrite(LED_PIN, LOW);
    Serial.println("LED 1: OFF");
  }

  int ldrValue = analogRead(ldrPin);
  ldrValue = map(ldrValue, 4095, 0, 1024, 0);
  float voltase = ldrValue / 1024.0 * 5;
  float resistansi = 2000 * voltase / (1-voltase / 5);
  float kecerahan = pow(RL10 * 1e3 * pow(10, GAMMA) / resistansi, (1/GAMMA));

  Serial.print("Lux: ");
  Serial.print(kecerahan);
  if(kecerahan > 50) {
    Serial.print(" Cahaya: Terang ");
    Serial.println("LED 2: OFF ");
    digitalWrite(ledPin, LOW);
  } else {
    Serial.print(" Cahaya: Gelap ");
    Serial.println("LED 2: ON ");
    digitalWrite(ledPin, HIGH);
  }

  char msg[150];
  snprintf(msg, 150, "{\"humidity\": %.2f, \"temperature\": %.2f, \"brightness\": %.2f}", h, t, kecerahan);
  client.publish("sensor/data", msg, 1);  // Publish with QoS 1

  // Check lamp control timing
  if (millis() - lampOnTime < lampDuration) {
    digitalWrite(ledPin, HIGH);  // Keep lamp ON
  } else if (millis() - lampOffTime < lampDuration) {
    digitalWrite(ledPin, LOW);   // Keep lamp OFF
  }

  delay(2000);
}
