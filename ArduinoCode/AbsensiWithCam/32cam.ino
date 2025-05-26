#include <esp_now.h>
#include <WiFi.h>
#include "esp_camera.h"
#include "base64.h"

// --- Pin Konfigurasi Kamera (AI-Thinker) ---
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
uint8_t esp32u_mac[] = {0x94, 0x54, 0xC5, 0x74, 0xE9, 0x60};  // Ganti sesuai alamat MAC ESP32U

// Struct metadata dan chunk base64
typedef struct {
  uint16_t fingerprint_id;
  uint32_t total_size;
  uint16_t total_chunks;
} __attribute__((packed)) ImageMetadata;

typedef struct {
  uint16_t index;
  char data[200];
} __attribute__((packed)) ImageChunk;

// Variabel global trigger
volatile uint16_t trigger_fingerprint_id = 0;
bool trigger_ready = false;

// Callback ketika data diterima
void onDataRecv(const esp_now_recv_info_t *recvInfo, const uint8_t *data, int len) {
  if (len >= sizeof(uint16_t)) {
    trigger_fingerprint_id = *(const uint16_t *)data;
    trigger_ready = true;
    Serial.printf("📥 Trigger received from basement: fingerprint_id = %d\n", trigger_fingerprint_id);
  } else {
    Serial.println("⚠️ Data received but too short");
  }
}

// Inisialisasi kamera
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
  config.jpeg_quality = 35;
  config.fb_count = 1;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("❌ Kamera gagal diinisialisasi");
    return false;
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  delay(1000);

  Serial.print("📡 ESP32-CAM MAC: ");
  Serial.println(WiFi.macAddress());

  if (!initCamera()) {
    Serial.println("❌ Gagal menginisialisasi kamera!");
    while (true) delay(1000);
  }

  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW init failed");
    return;
  }

  // Tambahkan ESP32U sebagai peer
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, esp32u_mac, 6);
  peerInfo.channel = 0;       // Default channel
  peerInfo.encrypt = false;

  if (!esp_now_is_peer_exist(esp32u_mac)) {
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("❌ Gagal menambahkan peer ESP32U");
      return;
    }
  }

  esp_now_register_recv_cb(onDataRecv);
  Serial.println("🤖 ESP32-CAM siap. Menunggu trigger dari basement...");
}

void loop() {
  if (trigger_ready) {
    trigger_ready = false;
    Serial.printf("✅ fingerprint_id diterima: %d\n", trigger_fingerprint_id);

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("❌ Gagal mengambil gambar dari kamera");
      return;
    }

    // Encode ke base64
    String base64Image = base64::encode(fb->buf, fb->len);
    esp_camera_fb_return(fb);  // Penting!

    uint16_t chunk_size = 200;
    uint16_t total_chunks = (base64Image.length() + chunk_size - 1) / chunk_size;

    // Kirim metadata
    ImageMetadata meta;
    meta.fingerprint_id = trigger_fingerprint_id;
    meta.total_size = base64Image.length();
    meta.total_chunks = total_chunks;

    esp_err_t result = esp_now_send(esp32u_mac, (uint8_t*)&meta, sizeof(meta));
    if (result == ESP_OK) {
      Serial.printf("📤 Metadata dikirim: size=%d bytes, chunks=%d\n", meta.total_size, meta.total_chunks);
    } else {
      Serial.printf("❌ Gagal kirim metadata (err=%d)\n", result);
      return;
    }

    delay(50); // jeda sebelum kirim chunk

    // Kirim semua chunk
    for (uint16_t i = 0; i < total_chunks; i++) {
      ImageChunk chunk;
      chunk.index = i;
      int start = i * chunk_size;
      String part = base64Image.substring(start, start + chunk_size);
      memset(chunk.data, 0, sizeof(chunk.data));
      part.toCharArray(chunk.data, sizeof(chunk.data));

      result = esp_now_send(esp32u_mac, (uint8_t*)&chunk, sizeof(chunk));
      if (result == ESP_OK) {
        Serial.printf("📦 Chunk %d/%d terkirim\n", i + 1, total_chunks);
      } else {
        Serial.printf("❌ Gagal kirim chunk %d (err=%d)\n", i, result);
      }

      delay(20);  // jeda antar chunk
    }

    Serial.println("✅ Semua data dikirim!");
  }

  delay(10);
}
