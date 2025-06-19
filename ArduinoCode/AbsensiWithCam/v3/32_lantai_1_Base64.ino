#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <base64.h>  // Gunakan versi ini untuk Base64

// ======================= CONFIG =======================
const char* ssid = "SSID";
const char* password = "PASSWORD";

const char* mqtt_server = "XXXXXXXXXXXXXXXXXX.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_username = "hivemq.webclient.XXXXXXXXXXXXXXXXX";
const char* mqtt_password = "XXXXXXXXXXXXXXXXXXXXXXX";

const char* topic_absen = "/absensi";
const char* topic_image = "/image/raw";
const char* topic_enroll = "/enroll";
const char* topic_hapus = "/hapus";

// ======================= MQTT =======================
WiFiClientSecure espClient;
PubSubClient client(espClient);

String serialBuffer = "";
int imageSizeExpected = 0;
int imageSizeReceived = 0;
int imageFingerprintID = -1;
std::vector<uint8_t> imageBuffer;
bool receivingImage = false;

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
    if (receivingImage) {
      int available = Serial2.available();
      while (available-- && imageSizeReceived < imageSizeExpected) {
        imageBuffer[imageSizeReceived++] = Serial2.read();
      }

      if (imageSizeReceived >= imageSizeExpected) {
        Serial.println("✅ Gambar lengkap diterima, encoding ke base64...");

        String base64Image = base64::encode(imageBuffer.data(), imageSizeExpected);

        StaticJsonDocument<30000> doc;
        doc["fingerprint_id"] = imageFingerprintID;
        doc["base64"] = base64Image;

        String output;
        serializeJson(doc, output);
        client.publish(topic_image, output.c_str());
        Serial.println("📤 Gambar dikirim ke MQTT /image/raw");

        receivingImage = false;
      }
      return; // tunggu hingga selesai
    }

    char ch = Serial2.read();
    if (ch == '\n') {
      handleSerialInput(serialBuffer);
      serialBuffer = "";
    } else {
      serialBuffer += ch;
    }
  }

  delay(5);
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
  }

  else if (input.startsWith("IMG_META:")) {
    int sep = input.indexOf('|');
    if (sep > 0) {
      imageFingerprintID = input.substring(9, sep).toInt();
      imageSizeExpected = input.substring(sep + 1).toInt();

      Serial.printf("📥 Menerima gambar dari ID %d, ukuran: %d byte\n", imageFingerprintID, imageSizeExpected);

      imageBuffer.clear();
      imageBuffer.resize(imageSizeExpected);
      imageSizeReceived = 0;
      receivingImage = true;
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

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<200> doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) {
    Serial.println("❌ JSON parse error");
    return;
  }

  if (!doc.containsKey("fingerprint_id")) return;

  int id = doc["fingerprint_id"];
  if (strcmp(topic, topic_enroll) == 0) {
    Serial.printf("📤 Kirim perintah ENROLL ID %d ke ESP32U\n", id);
    Serial2.printf("ENROLL:%d\n", id);
  } else if (strcmp(topic, topic_hapus) == 0) {
    Serial.printf("📤 Kirim perintah HAPUS ID %d ke ESP32U\n", id);
    Serial2.printf("HAPUS:%d\n", id);
  }
}
