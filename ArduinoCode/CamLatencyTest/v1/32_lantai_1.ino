#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ======================= CONFIG =======================
const char* ssid = ""; 
const char* password = ""; 

const char* mqtt_server = "XXXXXXXXXXXXXXXX.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_username = "hivemq.webclient.XXXXXXXXXX";
const char* mqtt_password = "XXXXXXXXXXXXXX";

const char* topic_enroll = "/enroll";
const char* topic_absen  = "/absensi";
const char* topic_hapus  = "/hapus";
const char* topic_image  = "/image/raw";
const char* topic_image_meta = "/image/meta";

WiFiClientSecure espClient;
PubSubClient client(espClient);

String serialBuffer = "";
bool receivingImage = false;
int expectedImageSize = 0;
int currentImageID = -1;
String currentImageTimestamp = "";
std::vector<uint8_t> imageBuffer;

// ======================= SETUP =======================
void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, 16, 17);  // RX = GPIO16, TX = GPIO17

  connectToWiFi();

  espClient.setInsecure(); // Skip certificate verification
  client.setServer(mqtt_server, mqtt_port);
  client.setBufferSize(12288);  // cukup besar untuk gambar
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
        Serial.printf("\xF0\x9F\x93\xB8 Gambar lengkap diterima dari ID %d, ukuran %d bytes\n", currentImageID, expectedImageSize);

        // Kirim metadata terlebih dahulu
        StaticJsonDocument<256> meta;
        meta["fingerprint_id"] = currentImageID;
        meta["timestamp"] = currentImageTimestamp;
        char buffer[256];
        serializeJson(meta, buffer);
        client.publish(topic_image_meta, buffer);

        // Kirim data mentah ke MQTT
        if (client.publish(topic_image, imageBuffer.data(), expectedImageSize)) {
          Serial.println("\xE2\x9C\x85 Gambar mentah berhasil dikirim ke MQTT /image/raw");
        } else {
          Serial.println("\xE2\x9D\x8C Gagal mengirim gambar mentah ke MQTT");
        }

        // Reset
        receivingImage = false;
        imageBuffer.clear();
        expectedImageSize = 0;
        currentImageID = -1;
        currentImageTimestamp = "";
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

      Serial.printf("\xF0\x9F\x93\xA5 Data absen: ID=%d, Waktu=%s\n", fingerprint_id, timestamp.c_str());

      StaticJsonDocument<200> doc;
      doc["fingerprint_id"] = fingerprint_id;
      doc["timestamp"] = timestamp;

      char payload[256];
      serializeJson(doc, payload);
      client.publish(topic_absen, payload);
      Serial.println("\xE2\x9C\x85 Data absen dikirim ke MQTT /absensi");
    }
  } else if (input.startsWith("IMG_META:")) {
    input.remove(0, 9);  // buang "IMG_META:"

    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, input);
    if (error) {
      Serial.println("\xE2\x9D\x8C JSON IMG_META parsing error");
      return;
    }

    currentImageID = doc["id"];
    expectedImageSize = doc["size"];
    currentImageTimestamp = doc["timestamp"].as<String>();

    Serial.printf("\xF0\x9F\x96\xBC Metadata gambar: ID=%d, Size=%d, Time=%s\n", currentImageID, expectedImageSize, currentImageTimestamp.c_str());

    receivingImage = true;
    imageBuffer.clear();
    imageBuffer.reserve(expectedImageSize);
  }
}

// ======================= WIFI & MQTT =======================
void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("\xF0\x9F\x94\x8C Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\n\xE2\x9C\x85 WiFi connected!");
}

void connectToMQTT() {
  while (!client.connected()) {
    Serial.print("\xF0\x9F\x94\x97 Connecting to MQTT...");
    if (client.connect("ESP32_DEVKIT_CLIENT", mqtt_username, mqtt_password)) {
      Serial.println("\xE2\x9C\x85 Connected to MQTT");
      client.subscribe(topic_enroll);
      client.subscribe(topic_hapus);
    } else {
      Serial.print("\xE2\x9D\x8C Failed, rc=");
      Serial.println(client.state());
      delay(3000);
    }
  }
}

// ======================= MQTT Callback =======================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.println("\n\xF0\x9F\x93\xA9 MQTT message received!");

  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  if (error) {
    Serial.print("\xE2\x9D\x8C JSON Error: ");
    Serial.println(error.f_str());
    return;
  }

  if (doc.containsKey("fingerprint_id")) {
    int id = doc["fingerprint_id"];

    if (strcmp(topic, topic_enroll) == 0) {
      Serial.printf("\xF0\x9F\x93\xA4 Kirim perintah ENROLL ID %d ke ESP32U\n", id);
      Serial2.printf("ENROLL:%d\n", id);

    } else if (strcmp(topic, topic_hapus) == 0) {
      Serial.printf("\xF0\x9F\x93\xA4 Kirim perintah HAPUS ID %d ke ESP32U\n", id);
      Serial2.printf("HAPUS:%d\n", id);
    }
  }
}
