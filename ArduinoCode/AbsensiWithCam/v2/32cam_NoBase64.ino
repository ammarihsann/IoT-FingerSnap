#include <esp_now.h>
#include <WiFi.h>
#include "esp_camera.h"
#include "esp_crc.h"  // Untuk CRC32

#define DEBUG 1  // Matikan dengan ubah ke 0

// --- Konfigurasi Kamera (AI-Thinker) ---
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// MAC Address ESP32U (tujuan)
uint8_t esp32u_mac[] = {0x94, 0x54, 0xC5, 0x74, 0xE9, 0x60};  // Ganti sesuai MAC tujuan

// Struct metadata gambar
typedef struct {
  uint16_t fingerprint_id;
  uint32_t total_size;
  uint16_t total_chunks;
} __attribute__((packed)) ImageMetadata;

// Struct chunk data
typedef struct {
  uint16_t index;
  uint16_t length;
  uint8_t data[230];
} __attribute__((packed)) ImageChunk;

// Struct akhir (opsional)
typedef struct {
  uint32_t crc32;
} __attribute__((packed)) ImageEnd;

volatile uint16_t trigger_fingerprint_id = 0;
bool trigger_ready = false;

// Callback saat data diterima (trigger)
void onDataRecv(const esp_now_recv_info_t *recvInfo, const uint8_t *data, int len) {
  if (len >= sizeof(uint16_t)) {
    trigger_fingerprint_id = *(const uint16_t *)data;
    trigger_ready = true;
#if DEBUG
    Serial.printf("📥 Trigger diterima: fingerprint_id = %d\n", trigger_fingerprint_id);
#endif
  }
}

bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 30;
  config.fb_count = 1;

  return esp_camera_init(&config) == ESP_OK;
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  delay(1000);

#if DEBUG
  Serial.print("📡 ESP32-CAM MAC: ");
  Serial.println(WiFi.macAddress());
#endif

  if (!initCamera()) {
    Serial.println("❌ Gagal inisialisasi kamera");
    while (true) delay(1000);
  }

  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW init gagal");
    return;
  }

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, esp32u_mac, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (!esp_now_is_peer_exist(esp32u_mac)) {
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("❌ Gagal tambah peer");
      return;
    }
  }

  esp_now_register_recv_cb(onDataRecv);

#if DEBUG
  Serial.println("🤖 ESP32-CAM siap, menunggu trigger...");
#endif
}

void loop() {
  if (trigger_ready) {
    trigger_ready = false;

#if DEBUG
    Serial.printf("📸 Trigger diproses: fingerprint_id = %d\n", trigger_fingerprint_id);
#endif

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("❌ Gagal ambil gambar");
      return;
    }

    uint32_t img_len = fb->len;
    uint8_t* img_buf = fb->buf;
    uint16_t chunk_size = 230;
    uint16_t total_chunks = (img_len + chunk_size - 1) / chunk_size;

    // Kirim metadata
    ImageMetadata meta;
    meta.fingerprint_id = trigger_fingerprint_id;
    meta.total_size = img_len;
    meta.total_chunks = total_chunks;

    if (esp_now_send(esp32u_mac, (uint8_t*)&meta, sizeof(meta)) != ESP_OK) {
      Serial.println("❌ Gagal kirim metadata");
      esp_camera_fb_return(fb);
      return;
    }

#if DEBUG
    Serial.printf("📤 Metadata: size=%u bytes, chunks=%u\n", img_len, total_chunks);
#endif

    delay(50);

    for (uint16_t i = 0; i < total_chunks; i++) {
      ImageChunk chunk;
      chunk.index = i;
      uint16_t bytes_left = img_len - (i * chunk_size);
      chunk.length = (bytes_left > chunk_size) ? chunk_size : bytes_left;
      memcpy(chunk.data, img_buf + i * chunk_size, chunk.length);

      bool success = false;
      for (int attempt = 0; attempt < 3 && !success; attempt++) {
        esp_err_t result = esp_now_send(esp32u_mac, (uint8_t*)&chunk, 4 + chunk.length);
        success = (result == ESP_OK);
        if (!success) delay(10);
      }

#if DEBUG
      if (success) {
        Serial.printf("📦 Chunk %d/%d terkirim (%d bytes)\n", i + 1, total_chunks, chunk.length);
      } else {
        Serial.printf("❌ Gagal kirim chunk %d\n", i);
      }
#endif
      delay(15);  // jeda antar chunk
    }

    // Hitung CRC32 dan kirim
    uint32_t crc = esp_crc32_le(0, img_buf, img_len);
    ImageEnd end;
    end.crc32 = crc;
    esp_now_send(esp32u_mac, (uint8_t*)&end, sizeof(end));

#if DEBUG
    Serial.printf("✅ Semua chunk terkirim. CRC32: 0x%08X\n", crc);
#endif

    esp_camera_fb_return(fb);
  }

  delay(10);
}
