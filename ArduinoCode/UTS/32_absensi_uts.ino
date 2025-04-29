#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>
#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED setup
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Buzzer di pin D4
#define BUZZER_PIN 4

// Fingerprint sensor (Serial2 - GPIO16 RX, GPIO17 TX)
HardwareSerial mySerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

// Struktur data fingerprint
typedef struct {
  int fingerprint_id;
} AbsensiData;

AbsensiData absensiData;

enum Mode {
  MODE_ABSEN,
  MODE_ENROLL
};

Mode currentMode = MODE_ABSEN;
bool enrollRequested = false;
uint8_t enrollId = 0;

// Untuk animasi titik
unsigned long lastIdleAnim = 0;
int idleAnimState = 0;

// Fungsi callback untuk data dari ESP-NOW
void onDataReceived(const esp_now_recv_info* info, const uint8_t* data, int len) {
  Serial.println("📥 Data diterima dari ESP32-U");

  if (len != sizeof(AbsensiData)) {
    Serial.println("⚠️ Data size mismatch!");
    return;
  }

  memcpy(&absensiData, data, sizeof(absensiData));

  if (absensiData.fingerprint_id > 0) {
    enrollRequested = true;
    enrollId = absensiData.fingerprint_id;
    currentMode = MODE_ENROLL;
    Serial.printf("📌 Enroll diminta untuk ID: %d\n", enrollId);
  }
}

// Fungsi tampilkan tulisan atas-bawah
void showOnOLED(const String& line2, const String& line3) {
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("Status:");

  display.setTextSize(2);
  display.setCursor(0, 16);
  display.println(line2);
  display.setCursor(0, 40);
  display.println(line3);

  display.display();
}

// Fungsi animasi titik berjalan saat idle
void showIdleAnimation() {
  unsigned long now = millis();
  if (now - lastIdleAnim > 500) {
    lastIdleAnim = now;
    idleAnimState = (idleAnimState + 1) % 4;

    String dots = "";
    for (int i = 0; i < idleAnimState; i++) dots += ".";

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.println("Status:");

    display.setTextSize(2);
    display.setCursor(0, 16);
    display.println("Siap");
    display.setCursor(0, 40);
    display.print("absensi");
    display.print(dots);

    display.display();
  }
}

// Fungsi beep buzzer
void beep(int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
  }
}

// Fungsi untuk enroll sidik jari
bool enrollFingerprint(uint8_t id) {
  int p;

  Serial.println("🖐️ Letakkan jari pertama...");
  while ((p = finger.getImage()) != FINGERPRINT_OK) {
    if (p == FINGERPRINT_NOFINGER) Serial.print(".");
    else return false;
    delay(100);
  }
  Serial.println("\n✅ Gambar pertama diambil");

  if (finger.image2Tz(1) != FINGERPRINT_OK) return false;

  Serial.println("👉 Angkat jari...");
  delay(2000);
  while (finger.getImage() != FINGERPRINT_NOFINGER) delay(100);

  Serial.println("🖐️ Letakkan jari lagi...");
  delay(2000);
  while ((p = finger.getImage()) != FINGERPRINT_OK) {
    if (p == FINGERPRINT_NOFINGER) Serial.print(".");
    else return false;
    delay(100);
  }
  Serial.println("\n✅ Gambar kedua diambil");

  if (finger.image2Tz(2) != FINGERPRINT_OK) return false;
  if (finger.createModel() != FINGERPRINT_OK) return false;
  if (finger.storeModel(id) != FINGERPRINT_OK) return false;

  return true;
}

void setup() {
  Serial.begin(115200);
  mySerial.begin(57600, SERIAL_8N1, 16, 17);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Inisialisasi OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("❌ OLED tidak ditemukan"));
    while (true) delay(1000);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("Absensi Fingerprint");
  display.display();

  // Inisialisasi ESP-NOW
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW gagal init");
    return;
  }
  esp_now_register_recv_cb(onDataReceived);
  Serial.println("✅ ESP-NOW siap menerima data");

  // Cek sensor fingerprint
  if (finger.verifyPassword()) {
    Serial.println("✅ Sensor fingerprint terdeteksi");
  } else {
    Serial.println("❌ Fingerprint sensor tidak ditemukan!");
    while (true) delay(1000);
  }
}

void loop() {
  if (currentMode == MODE_ENROLL && enrollRequested) {
    Serial.println("🔁 MODE_ENROLL aktif...");
    if (enrollFingerprint(enrollId)) {
      Serial.println("✅ Enroll sukses");
      showOnOLED("Enroll", "sukses");
      beep(2);
    } else {
      Serial.println("❌ Enroll gagal");
      showOnOLED("Enroll", "gagal");
      beep(1);
    }
    currentMode = MODE_ABSEN;
    enrollRequested = false;
    enrollId = 0;
    delay(2000);
  }

  if (currentMode == MODE_ABSEN) {
    int p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) {
      showIdleAnimation();
      delay(100);
      return;
    }
    if (p != FINGERPRINT_OK) {
      delay(500);
      return;
    }

    p = finger.image2Tz();
    if (p != FINGERPRINT_OK) {
      delay(500);
      return;
    }

    p = finger.fingerSearch();
    if (p == FINGERPRINT_OK) {
      Serial.printf("👤 Cocok! ID: %d\n", finger.fingerID);
      showOnOLED("Absensi", "Berhasil!");
      beep(2);
    } else {
      Serial.println("🙅 Tidak ditemukan");
      showOnOLED("Absensi", "Gagal!");
      beep(1);
    }

    delay(2000);
  }
}
