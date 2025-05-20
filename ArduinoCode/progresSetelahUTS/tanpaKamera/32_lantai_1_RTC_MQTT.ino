#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ======================= WiFi & MQTT CONFIG =======================
const char* ssid = "";
const char* password = "";

const char* mqtt_server = "";
const int mqtt_port = 8883;
const char* mqtt_username = "";
const char* mqtt_password = "";

const char* topic_enroll = "/enroll";
const char* topic_absen  = "/absensi";

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
  if (!client.connected()) {
    connectToMQTT();
  }
  client.loop();

  // 🔄 Cek jika ada data dari ESP32U (via Serial2)
  if (Serial2.available()) {
    String input = Serial2.readStringUntil('\n');
    input.trim();

    if (input.startsWith("ABSEN:")) {
      // Format: ABSEN:123|2025-05-16 10:10:10
      int sep = input.indexOf('|');
      if (sep > 0) {
        String idStr = input.substring(6, sep);
        String timestamp = input.substring(sep + 1);

        int fingerprint_id = idStr.toInt();

        Serial.printf("📥 Data diterima dari ESP32U: ID %d, Waktu %s\n", fingerprint_id, timestamp.c_str());

        // Kirim ke MQTT
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
    Serial.printf("📤 Kirim perintah enroll ID %d ke ESP32U via Serial2\n", id);
    Serial2.printf("ENROLL:%d\n", id);
  }
}
