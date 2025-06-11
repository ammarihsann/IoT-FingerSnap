#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ======================= CONFIG =======================
const char* ssid = "";
const char* password = "";

const char* mqtt_server = "XXXXXXXXXXXXXXX.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_username = "hivemq.webclient.XXXXXXXXXXX";
const char* mqtt_password = "";

const char* topic_enroll = "/enroll";
const char* topic_absen  = "/absensi";
const char* topic_hapus  = "/hapus";
const char* topic_image  = "/image/raw";

WiFiClientSecure espClient;
PubSubClient client(espClient);

String serialBuffer = "";
bool receivingImage = false;
int expectedImageSize = 0;
int currentImageID = -1;
std::vector<uint8_t> imageBuffer;

// ======================= SETUP =======================
void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, 16, 17);  // RX = GPIO16, TX = GPIO17

  connectToWiFi();

  espClient.setInsecure(); // Skip certificate verification
  client.setServer(mqtt_server, mqtt_port);
  client.setBufferSize(12288);  // pastikan cukup besar untuk image
  client.setCallback(mqttCallback);

  connectToMQTT();
}

// ======================= LOOP =======================
void loop() {
  if (WiFi.status() != WL_CONNECTED) connectToWiFi();
  if (!client.connected()) connectToMQTT();
  client.loop();

  while (Serial2.available()) {
    if (!receivingImage) {
      char ch = Serial2.read();
      if (ch == '\n') {
        handleSerialInput(serialBuffer);
        serialBuffer = "";
      } else {
        serialBuffer += ch;
      }
    } else {
      // Terima data binary gambar
      while (Serial2.available() && (int)imageBuffer.size() < expectedImageSize) {
        imageBuffer.push_back(Serial2.read());
      }

      if ((int)imageBuffer.size() >= expectedImageSize) {
        Serial.printf("📸 Gambar lengkap diterima dari ID %d, ukuran %d bytes\n", currentImageID, expectedImageSize);

        // Kirim data mentah ke MQTT
        if (client.publish(topic_image, imageBuffer.data(), expectedImageSize)) {
          Serial.println("✅ Gambar mentah berhasil dikirim ke MQTT /image/raw");
        } else {
          Serial.println("❌ Gagal mengirim gambar mentah ke MQTT");
        }

        // Reset
        receivingImage = false;
        imageBuffer.clear();
        expectedImageSize = 0;
        currentImageID = -1;
      }
    }
  }
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
  } else if (input.startsWith("IMG_META:")) {
    // Format: IMG_META:<id>|<size>
    int sep = input.indexOf('|');
    if (sep > 8) {
      currentImageID = input.substring(9, sep).toInt();
      expectedImageSize = input.substring(sep + 1).toInt();

      Serial.printf("🖼 Metadata gambar diterima: ID=%d, Size=%d bytes\n", currentImageID, expectedImageSize);

      receivingImage = true;
      imageBuffer.clear();
      imageBuffer.reserve(expectedImageSize);
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

    if (strcmp(topic, topic_enroll) == 0) {
      Serial.printf("📤 Kirim perintah ENROLL ID %d ke ESP32U\n", id);
      Serial2.printf("ENROLL:%d\n", id);

    } else if (strcmp(topic, topic_hapus) == 0) {
      Serial.printf("📤 Kirim perintah HAPUS ID %d ke ESP32U\n", id);
      Serial2.printf("HAPUS:%d\n", id);
    }
  }
}
