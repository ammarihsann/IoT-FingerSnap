#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ======================= CONFIG =======================
const char* ssid = "SSID";
const char* password = "PASSWORD";

const char* mqtt_server = "XXXXXXX.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_username = "hivemq.webclient.XXXXXXXX";
const char* mqtt_password = "PASSWORD";

const char* topic_enroll = "/enroll";
const char* topic_absen  = "/absensi";
const char* topic_hapus  = "/hapus";
const char* topic_image  = "/image/raw";

WiFiClientSecure espClient;
PubSubClient client(espClient);

String serialBuffer = "";

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, 16, 17);  // RX = GPIO16, TX = GPIO17

  connectToWiFi();

  espClient.setInsecure(); // Skip certificate verification
  client.setServer(mqtt_server, mqtt_port);
  client.setBufferSize(12288);
  client.setCallback(mqttCallback);

  connectToMQTT();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectToWiFi();
  if (!client.connected()) connectToMQTT();
  client.loop();

  while (Serial2.available()) {
    char ch = Serial2.read();
    if (ch == '\n') {
      handleSerialInput(serialBuffer);
      serialBuffer = "";
    } else {
      serialBuffer += ch;
    }
  }

  delay(10);
}

// ======================= HANDLER =======================
void handleSerialInput(String input) {
  input.trim();

  if (input.startsWith("ABSEN:")) {
    int sep = input.indexOf('|');
    if (sep > 0) {
      int fingerprint_id = input.substring(6, sep).toInt();
      String timestamp = input.substring(sep + 1);

      Serial.printf("📥 Data absen: ID=%d, Waktu=%s\n", fingerprint_id, timestamp.c_str());

      StaticJsonDocument<200> doc;
      doc["fingerprint_id"] = fingerprint_id;
      doc["timestamp"] = timestamp;

      char payload[256];
      serializeJson(doc, payload);
      client.publish(topic_absen, payload);
      Serial.println("✅ Data absen dikirim ke MQTT /absensi");
    }

  } else if (input.startsWith("IMG:")) {
    String jsonStr = input.substring(4);

    Serial.println("📩 Gambar diterima dari ESP32U:");
    Serial.println(jsonStr.substring(0, 100) + "...");  // preview awal saja

    // Gunakan heap agar aman dari overflow
    DynamicJsonDocument doc(14 * 1024);  // alokasikan 14KB heap
    DeserializationError error = deserializeJson(doc, jsonStr);
    if (error) {
      Serial.print("❌ Gagal parsing JSON gambar: ");
      Serial.println(error.f_str());
      return;
    }

    int fingerprint_id = doc["fingerprint_id"];
    const char* base64 = doc["base64"];
    int base64_len = strlen(base64);

    if (base64_len > 20000) {
      Serial.println("⚠ Gambar terlalu besar, abaikan untuk mencegah crash");
      return;
    }

    Serial.printf("📷 Gambar dari ID %d, panjang base64: %d byte\n", fingerprint_id, base64_len);

    // Kirim ke MQTT langsung
    bool success = client.publish(topic_image, jsonStr.c_str());
    if (success) {
      Serial.println("✅ Gambar berhasil dikirim ke MQTT /image/raw");
    } else {
      Serial.println("❌ Gagal mengirim gambar ke MQTT");
    }
  }
}

// ======================= WIFI & MQTT =======================
void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("🔌 Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\n✅ WiFi connected!");
}

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

// ======================= MQTT Callback =======================
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
      Serial.printf("📤 Kirim perintah ENROLL ID %d ke ESP32U\n", id);
      Serial2.printf("ENROLL:%d\n", id);

    } else if (strcmp(topic, "/hapus") == 0) {
      Serial.printf("📤 Kirim perintah HAPUS ID %d ke ESP32U\n", id);
      Serial2.printf("HAPUS:%d\n", id);
    }
  }
}
