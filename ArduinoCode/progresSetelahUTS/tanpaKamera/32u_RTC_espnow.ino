#include <esp_now.h>
#include <WiFi.h>

// Struct hasil absensi (dari Basement)
typedef struct {
  int fingerprint_id;
  char timestamp[25];
} AbsensiData;

// Struct perintah enroll (ke Basement)
typedef struct {
  int fingerprint_id;
} EnrollCommand;

AbsensiData dataReceived;
uint8_t esp32BasementMac[] = { 0xCC, 0xDB, 0xA7, 0x3D, 0xD4, 0xDC };

#define SERIAL_BAUDRATE 115200
#define SERIAL_DEVKIT_RX 16
#define SERIAL_DEVKIT_TX 17

void onDataReceived(const esp_now_recv_info* info, const uint8_t* data, int len) {
  if (len == sizeof(AbsensiData)) {
    memcpy(&dataReceived, data, sizeof(AbsensiData));
    Serial.println("📥 Data diterima dari Basement:");
    Serial.printf("   ID: %d, Waktu: %s\n", dataReceived.fingerprint_id, dataReceived.timestamp);
    Serial2.printf("ABSEN:%d|%s\n", dataReceived.fingerprint_id, dataReceived.timestamp);
  } else {
    Serial.println("⚠️ Ukuran data tidak cocok");
  }
}

void setup() {
  Serial.begin(SERIAL_BAUDRATE);
  Serial2.begin(9600, SERIAL_8N1, SERIAL_DEVKIT_RX, SERIAL_DEVKIT_TX);
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW init gagal");
    return;
  }

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, esp32BasementMac, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (!esp_now_is_peer_exist(esp32BasementMac)) {
    if (esp_now_add_peer(&peerInfo) == ESP_OK) {
      Serial.println("✅ Peer Basement ditambahkan");
    } else {
      Serial.println("❌ Gagal menambahkan peer");
    }
  }

  esp_now_register_recv_cb(onDataReceived);
}

void loop() {
  if (Serial2.available()) {
    String input = Serial2.readStringUntil('\n');
    input.trim();

    if (input.startsWith("ENROLL:")) {
      int id = input.substring(7).toInt();
      EnrollCommand cmd;
      cmd.fingerprint_id = id;
      esp_err_t result = esp_now_send(esp32BasementMac, (uint8_t*)&cmd, sizeof(cmd));
      Serial.printf("📤 Enroll ID %d dikirim ke Basement: %s\n", id, result == ESP_OK ? "✅ Sukses" : "❌ Gagal");
    }
  }
}
