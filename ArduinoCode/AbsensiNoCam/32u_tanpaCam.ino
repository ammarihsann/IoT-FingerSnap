#include <esp_now.h>
#include <WiFi.h>

// Struct data absensi dari Basement
typedef struct {
  int fingerprint_id;
  char timestamp[25];
} AbsensiData;

// Struct perintah enroll ke Basement
typedef struct {
  int fingerprint_id;
} EnrollCommand;

AbsensiData dataReceived;
uint8_t esp32BasementMac[] = { 0xCC, 0xDB, 0xA7, 0x3D, 0xD4, 0xDC }; // Ganti sesuai MAC Basement

#define SERIAL_BAUDRATE 115200
#define SERIAL_DEVKIT_RX 16
#define SERIAL_DEVKIT_TX 17

void onDataReceived(const esp_now_recv_info* info, const uint8_t* data, int len) {
  if (len == sizeof(AbsensiData)) {
    memcpy(&dataReceived, data, sizeof(AbsensiData));
    Serial.println("📥 Data absensi diterima dari Basement:");
    Serial.printf("   ID: %d, Waktu: %s\n", dataReceived.fingerprint_id, dataReceived.timestamp);

    // Kirim ke ESP32 lantai 1 lewat Serial2
    Serial2.printf("ABSEN:%d|%s\n", dataReceived.fingerprint_id, dataReceived.timestamp);
  } else {
    Serial.println("⚠️ Data yang diterima tidak sesuai ukuran");
  }
}

void setup() {
  Serial.begin(SERIAL_BAUDRATE);
  Serial2.begin(9600, SERIAL_8N1, SERIAL_DEVKIT_RX, SERIAL_DEVKIT_TX);
  WiFi.mode(WIFI_STA);

  // Inisialisasi ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ Gagal inisialisasi ESP-NOW");
    while (true) { delay(1000); }
  }

  // Tambah peer Basement
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, esp32BasementMac, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (!esp_now_is_peer_exist(esp32BasementMac)) {
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("❌ Gagal menambahkan peer Basement");
      while (true) { delay(1000); }
    }
  }
  esp_now_register_recv_cb(onDataReceived);

  Serial.println("✅ ESP32U siap menerima data dari Serial2 dan ESP-NOW");
}

void loop() {
  // Baca data dari ESP32 lantai 1 via Serial2
  if (Serial2.available()) {
    String input = Serial2.readStringUntil('\n');
    input.trim();

    Serial.print("Dari Serial2: ");
    Serial.println(input);

    if (input.startsWith("ENROLL:")) {
      int id = input.substring(7).toInt();
      Serial.printf("📤 Kirim perintah enroll ID %d ke Basement via ESP-NOW\n", id);

      EnrollCommand cmd;
      cmd.fingerprint_id = id;
      esp_err_t result = esp_now_send(esp32BasementMac, (uint8_t*)&cmd, sizeof(cmd));
      if (result == ESP_OK) {
        Serial.println("✅ Perintah enroll terkirim");
      } else {
        Serial.println("❌ Gagal kirim perintah enroll");
      }
    }
      else if (input.startsWith("HAPUS:")) {
      int id = input.substring(6).toInt();
      Serial.printf("📤 Kirim perintah hapus ID %d ke Basement via ESP-NOW\n", id);

      EnrollCommand cmd;
      cmd.fingerprint_id = id;
      // Kirim dengan ID negatif sebagai penanda perintah hapus
      cmd.fingerprint_id = -abs(id);  
      esp_err_t result = esp_now_send(esp32BasementMac, (uint8_t*)&cmd, sizeof(cmd));
      if (result == ESP_OK) {
        Serial.println("✅ Perintah hapus terkirim");
      } else {
        Serial.println("❌ Gagal kirim perintah hapus");
      }
    }

  }
}
