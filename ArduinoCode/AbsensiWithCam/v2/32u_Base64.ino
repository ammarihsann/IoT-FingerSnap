#include <esp_now.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <base64.h>
#include <vector>

#define SERIAL_BAUDRATE 115200
#define SERIAL_DEVKIT_RX 16
#define SERIAL_DEVKIT_TX 17

// MAC Address - sesuaikan dengan perangkatmu
uint8_t esp32CamMac[] = {0x94, 0x54, 0xC5, 0x74, 0xE9, 0x60};
uint8_t esp32BasementMac[] = {0xCC, 0xDB, 0xA7, 0x3D, 0xD4, 0xDC};

// Struct AbsensiData
typedef struct {
  int fingerprint_id;
  char timestamp[25];
} AbsensiData;

// Struct EnrollCommand
typedef struct {
  int fingerprint_id;
} EnrollCommand;

// Struct Metadata Gambar
typedef struct __attribute__((packed)) {
  uint16_t fingerprint_id;
  uint32_t total_size;
  uint16_t total_chunks;
} ImageMetadata;

// Struct Akhir (CRC)
typedef struct __attribute__((packed)) {
  uint32_t crc32;
} ImageEnd;

AbsensiData dataReceived;
bool receivingImage = false;
std::vector<uint8_t> imageBuffer;
uint16_t expectedChunks = 0;
uint16_t receivedChunks = 0;
uint16_t currentFingerprint = 0;
const uint16_t chunkPayloadSize = 230;  // Sesuai dengan ESP32-CAM

void onDataReceived(const esp_now_recv_info* info, const uint8_t* data, int len) {
  if (len == sizeof(AbsensiData)) {
    memcpy(&dataReceived, data, sizeof(AbsensiData));
    Serial.printf("📥 Absensi: ID=%d, Time=%s\n", dataReceived.fingerprint_id, dataReceived.timestamp);
    Serial2.printf("ABSEN:%d|%s\n", dataReceived.fingerprint_id, dataReceived.timestamp);
    return;
  }

  if (len == sizeof(ImageMetadata)) {
    ImageMetadata meta;
    memcpy(&meta, data, sizeof(ImageMetadata));
    Serial.printf("🖼 Metadata diterima: ID=%d, Size=%lu, Chunks=%d\n", meta.fingerprint_id, meta.total_size, meta.total_chunks);

    imageBuffer.clear();
    imageBuffer.resize(meta.total_size);  // Alokasikan ukuran sesuai total size
    receivingImage = true;
    expectedChunks = meta.total_chunks;
    receivedChunks = 0;
    currentFingerprint = meta.fingerprint_id;
    return;
  }

  if (receivingImage && len >= 4) {
    uint16_t chunkIndex = data[0] | (data[1] << 8);
    uint16_t chunkLength = data[2] | (data[3] << 8);
    if (len != 4 + chunkLength) {
      Serial.printf("⚠ Panjang chunk tidak cocok: Diterima %d, Diharapkan %d\n", len, 4 + chunkLength);
      return;
    }

    if ((chunkIndex * chunkPayloadSize + chunkLength) > imageBuffer.size()) {
      Serial.println("❌ Indeks chunk melebihi ukuran buffer");
      return;
    }

    memcpy(&imageBuffer[chunkIndex * chunkPayloadSize], &data[4], chunkLength);
    receivedChunks++;

    Serial.printf("📦 Chunk [%d/%d] diterima (%d bytes)\n", receivedChunks, expectedChunks, chunkLength);

    if (receivedChunks == expectedChunks) {
      receivingImage = false;
      Serial.println("✅ Semua chunk diterima. Konversi base64...");

      // Konversi ke Base64
      String base64Image = base64::encode(imageBuffer.data(), imageBuffer.size());

      StaticJsonDocument<8192> doc;
      doc["fingerprint_id"] = currentFingerprint;
      doc["base64"] = base64Image;

      String jsonStr;
      serializeJson(doc, jsonStr);

      Serial2.print("IMG:");
      Serial2.println(jsonStr);
      Serial.println("📤 Gambar base64 dikirim ke lantai 1");
    }
    return;
  }

  if (len == sizeof(ImageEnd)) {
    ImageEnd end;
    memcpy(&end, data, sizeof(end));
    Serial.printf("🔚 CRC32 dari pengirim: 0x%08X (belum diverifikasi)\n", end.crc32);
    return;
  }

  Serial.printf("⚠ Data tidak dikenali (len=%d)\n", len);
}

void setup() {
  Serial.begin(SERIAL_BAUDRATE);
  Serial2.begin(9600, SERIAL_8N1, SERIAL_DEVKIT_RX, SERIAL_DEVKIT_TX);
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ Gagal inisialisasi ESP-NOW");
    while (true) delay(1000);
  }

  // Tambahkan peer Basement
  esp_now_peer_info_t peer1 = {};
  memcpy(peer1.peer_addr, esp32BasementMac, 6);
  peer1.channel = 0;
  peer1.encrypt = false;
  if (!esp_now_is_peer_exist(esp32BasementMac)) esp_now_add_peer(&peer1);

  // Tambahkan peer ESP32-CAM
  esp_now_peer_info_t peer2 = {};
  memcpy(peer2.peer_addr, esp32CamMac, 6);
  peer2.channel = 0;
  peer2.encrypt = false;
  if (!esp_now_is_peer_exist(esp32CamMac)) esp_now_add_peer(&peer2);

  esp_now_register_recv_cb(onDataReceived);
  Serial.println("✅ ESP32U siap menerima data dari ESP-NOW & Serial2");
}

void loop() {
  if (Serial2.available()) {
    String input = Serial2.readStringUntil('\n');
    input.trim();

    if (input.startsWith("ENROLL:")) {
      int id = input.substring(7).toInt();
      EnrollCommand cmd = { .fingerprint_id = id };
      esp_now_send(esp32BasementMac, (uint8_t*)&cmd, sizeof(cmd));
      Serial.printf("📤 ENROLL ID %d dikirim ke Basement\n", id);
    }
    else if (input.startsWith("HAPUS:")) {
      int id = input.substring(6).toInt();
      EnrollCommand cmd = { .fingerprint_id = -abs(id) };
      esp_now_send(esp32BasementMac, (uint8_t*)&cmd, sizeof(cmd));
      Serial.printf("📤 HAPUS ID %d dikirim ke Basement\n", id);
    }
  }
}
