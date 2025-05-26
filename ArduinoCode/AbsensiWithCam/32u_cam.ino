#include <esp_now.h>
#include <WiFi.h>
#include <ArduinoJson.h>

#define SERIAL_BAUDRATE 115200
#define SERIAL_DEVKIT_RX 16
#define SERIAL_DEVKIT_TX 17

#define IMAGE_CHUNK_SIZE 200

// MAC ESP32-CAM
uint8_t esp32CamMac[] = {0x94, 0x54, 0xC5, 0x74, 0xE9, 0x60};  // Ganti sesuai MAC ESP32CAM
// MAC ESP32-Basement
uint8_t esp32BasementMac[] = { 0xCC, 0xDB, 0xA7, 0x3D, 0xD4, 0xDC }; // Ganti sesuai MAC

// Struct data absensi
typedef struct {
  int fingerprint_id;
  char timestamp[25];
} AbsensiData;

// Struct untuk command
typedef struct {
  int fingerprint_id;
} EnrollCommand;

// Struct metadata gambar
typedef struct __attribute__((packed)) {
  uint16_t fingerprint_id;
  uint32_t total_size;
  uint16_t total_chunks;
} ImageMetadata;

// Struct chunk gambar
typedef struct __attribute__((packed)) {
  uint16_t index;
  char data[IMAGE_CHUNK_SIZE];
} ImageChunk;

// Variabel Global
AbsensiData dataReceived;

bool receivingImage = false;
String base64Image = "";
uint16_t expectedChunks = 0;
uint16_t receivedChunks = 0;
uint16_t currentFingerprint = 0;

void onDataReceived(const esp_now_recv_info* info, const uint8_t* data, int len) {
  if (len == sizeof(AbsensiData)) {
    memcpy(&dataReceived, data, sizeof(AbsensiData));
    Serial.println("📥 Data absensi diterima dari Basement:");
    Serial.printf("   ID: %d, Waktu: %s\n", dataReceived.fingerprint_id, dataReceived.timestamp);
    Serial2.printf("ABSEN:%d|%s\n", dataReceived.fingerprint_id, dataReceived.timestamp);
    return;
  }

  if (len == sizeof(ImageMetadata)) {
    ImageMetadata meta;
    memcpy(&meta, data, sizeof(ImageMetadata));
    Serial.printf("🖼 Menerima metadata gambar - ID: %d, Size: %lu, Chunks: %d\n", meta.fingerprint_id, meta.total_size, meta.total_chunks);

    base64Image = "";
    receivingImage = true;
    expectedChunks = meta.total_chunks;
    receivedChunks = 0;
    currentFingerprint = meta.fingerprint_id;
    return;
  }

  if (len == sizeof(ImageChunk)) {
    if (!receivingImage) {
      Serial.println("⚠ Chunk diterima tapi metadata belum ada!");
      return;
    }

    ImageChunk chunk;
    memcpy(&chunk, data, sizeof(ImageChunk));

    base64Image += String(chunk.data);
    receivedChunks++;

    Serial.printf("📦 Chunk %d diterima (%d/%d)\n", chunk.index, receivedChunks, expectedChunks);

    if (receivedChunks == expectedChunks) {
      receivingImage = false;
      Serial.println("✅ Gambar selesai diterima");

      // Kirim gambar base64 dalam format JSON ke lantai 1
      StaticJsonDocument<6144> doc;
      doc["fingerprint_id"] = currentFingerprint;
      doc["base64"] = base64Image;

      String jsonStr;
      serializeJson(doc, jsonStr);
      Serial2.print("IMG:");
      Serial2.println(jsonStr);

      Serial.println("📤 Gambar base64 dikirim ke Serial2");
    }

    return;
  }

  Serial.printf("⚠ Data tidak dikenali, panjang: %d\n", len);
}

void setup() {
  Serial.begin(SERIAL_BAUDRATE);
  Serial2.begin(9600, SERIAL_8N1, SERIAL_DEVKIT_RX, SERIAL_DEVKIT_TX);
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ Gagal inisialisasi ESP-NOW");
    while (true) { delay(1000); }
  }

  // Peer Basement
  esp_now_peer_info_t peer1 = {};
  memcpy(peer1.peer_addr, esp32BasementMac, 6);
  peer1.channel = 0;
  peer1.encrypt = false;
  if (!esp_now_is_peer_exist(esp32BasementMac)) {
    esp_now_add_peer(&peer1);
  }

  // Peer ESP32CAM
  esp_now_peer_info_t peer2 = {};
  memcpy(peer2.peer_addr, esp32CamMac, 6);
  peer2.channel = 0;
  peer2.encrypt = false;
  if (!esp_now_is_peer_exist(esp32CamMac)) {
    esp_now_add_peer(&peer2);
  }

  esp_now_register_recv_cb(onDataReceived);
  Serial.println("✅ ESP32U siap menerima data dari ESP-NOW dan Serial2");
}

void loop() {
  if (Serial2.available()) {
    String input = Serial2.readStringUntil('\n');
    input.trim();

    Serial.print("📨 Dari Serial2: ");
    Serial.println(input);

    if (input.startsWith("ENROLL:")) {
      int id = input.substring(7).toInt();
      Serial.printf("📤 Kirim perintah ENROLL ID %d ke Basement\n", id);

      EnrollCommand cmd;
      cmd.fingerprint_id = id;
      esp_now_send(esp32BasementMac, (uint8_t*)&cmd, sizeof(cmd));
    }
    else if (input.startsWith("HAPUS:")) {
      int id = input.substring(6).toInt();
      Serial.printf("📤 Kirim perintah HAPUS ID %d ke Basement\n", id);

      EnrollCommand cmd;
      cmd.fingerprint_id = -abs(id);  // ID negatif untuk hapus
      esp_now_send(esp32BasementMac, (uint8_t*)&cmd, sizeof(cmd));
    }
  }
}
