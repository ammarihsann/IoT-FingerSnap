#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ======================= WiFi & MQTT CONFIG =======================
const char* ssid = "ASARM Family";
const char* password = "17121997";

const char* mqtt_server = "20b4045b4af5432c9978e9c8d1f97a15.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_username = "hivemq.webclient.1743687647826";
const char* mqtt_password = "x:WJf80AtIl5UagY$;1>";

const char* topic_enroll = "/enroll";
const char* topic_absen  = "/absensi";
const char* topic_hapus  = "/hapus";

// ======================= MQTT OBJECTS =======================
WiFiClientSecure espClient;
PubSubClient client(espClient);

// ======================= SETUP =======================
void setup() {
  Serial.begin(115200);     // Debug serial (USB)
  Serial2.begin(9600, SERIAL_8N1, 16, 17);  // RX=16, TX=17

  connectToWiFi();

  espClient.setInsecure();  // Gunakan TLS tanpa verifikasi sertifikat
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);

  connectToMQTT();
}

// ======================= LOOP =======================
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }

  if (!client.connected()) {
    connectToMQTT();
  }
  client.loop();

  // 🔄 Cek jika ada data dari ESP32U (via Serial2)
  if (Serial2.available()) {
    String input = Serial2.readStringUntil('\n');
    input.trim();

    if (input.startsWith("ABSEN:")) {
      int sep = input.indexOf('|');
      if (sep > 0) {
        String idStr = input.substring(6, sep);
        String timestamp = input.substring(sep + 1);

        int fingerprint_id = idStr.toInt();

        Serial.printf("📥 Data diterima dari ESP32U: ID %d, Waktu %s\n", fingerprint_id, timestamp.c_str());

        StaticJsonDocument<200> doc;
        doc["fingerprint_id"] = fingerprint_id;
        doc["timestamp"] = timestamp;

        char payload[256];
        serializeJson(doc, payload);
        client.publish(topic_absen, payload);
        Serial.println("✅ Data dikirim ke MQTT /absensi");
      }
    }
  }
}

// ======================= WIFI FUNCTION =======================
void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("🔌 Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\n✅ WiFi connected!");
}

// ======================= MQTT CONNECT FUNCTION =======================
void connectToMQTT() {
  while (!client.connected()) {
    Serial.print("🔗 Connecting to MQTT...");
    if (client.connect("ESP32_DEVKIT_CLIENT", mqtt_username, mqtt_password)) {
      Serial.println("✅ Connected to MQTT");
      client.subscribe(topic_enroll);
      client.subscribe(topic_hapus);
    } else {
      Serial.print("❌ Failed, rc=");
      Serial.println(client.state());
      delay(3000);
    }
  }
}

// ======================= MQTT CALLBACK =======================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.println("\n📩 MQTT message received!");

  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  if (error) {
    Serial.print("❌ JSON Error: ");
    Serial.println(error.f_str());
    return;
  }

  if (doc.containsKey("fingerprint_id")) {
    int id = doc["fingerprint_id"];

    if (strcmp(topic, "/enroll") == 0) {
      Serial.printf("📤 Kirim perintah enroll ID %d ke ESP32U via Serial2\n", id);
      Serial2.printf("ENROLL:%d\n", id);
    } else if (strcmp(topic, "/hapus") == 0) {
      Serial.printf("📤 Kirim perintah hapus ID %d ke ESP32U via Serial2\n", id);
      Serial2.printf("HAPUS:%d\n", id);
    }
  }
}
