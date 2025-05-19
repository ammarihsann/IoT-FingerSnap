#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>
#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>

// ====================== OLED SETUP ======================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ====================== RTC ======================
RTC_DS3231 rtc;

// ====================== BUZZER ======================
#define BUZZER_PIN 2

// ====================== FINGERPRINT ======================
HardwareSerial mySerial(2); // RX=16, TX=17
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

// ====================== ESP-NOW ======================
uint8_t esp32uMac[] = { 0x94, 0x54, 0xC5, 0x74, 0xE9, 0x60 };

// Struct untuk hasil absensi
typedef struct {
  int fingerprint_id;
  char timestamp[25];
} AbsensiData;

// Struct untuk perintah enroll
typedef struct {
  int fingerprint_id;
} EnrollCommand;

AbsensiData absensiData;
bool enrollRequested = false;
uint8_t enrollId = 0;

enum Mode { MODE_ABSEN, MODE_ENROLL };
Mode currentMode = MODE_ABSEN;

// ====================== FUNGSI ======================
void beep(int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
  }
}

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

void showIdleAnimation() {
  DateTime now = rtc.now();
  char timeStr[9], dateStr[11];
  sprintf(timeStr, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  sprintf(dateStr, "%02d-%02d-%04d", now.day(), now.month(), now.year());
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("Waktu:");
  display.setTextSize(2);
  display.setCursor(0, 16);
  display.println(timeStr);
  display.setTextSize(1);
  display.setCursor(0, 48);
  display.println(dateStr);
  display.display();
}

void showEnrollStatus(const String& status) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("Enroll mode:");
  display.setTextSize(2);
  display.setCursor(0, 20);
  display.println(status);
  display.display();
}


bool enrollFingerprint(uint8_t id) {
  int p;
  Serial.println("🖐️ Letakkan jari pertama...");
  showEnrollStatus("Letakkan  jari");
  while ((p = finger.getImage()) != FINGERPRINT_OK) {
    if (p == FINGERPRINT_NOFINGER) Serial.print(".");
    else return false;
    delay(100);
  }
  Serial.println("\n✅ Gambar pertama diambil");

  if (finger.image2Tz(1) != FINGERPRINT_OK) return false;

  Serial.println("👉 Angkat jari...");
  showEnrollStatus("Angkat    jari");
  delay(2000);
  while (finger.getImage() != FINGERPRINT_NOFINGER) delay(100);

  Serial.println("🖐️ Letakkan jari lagi...");
  showEnrollStatus("Letakkan  jari lagi");
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

void onDataReceived(const esp_now_recv_info* info, const uint8_t* data, int len) {
  if (len == sizeof(EnrollCommand)) {
    EnrollCommand cmd;
    memcpy(&cmd, data, sizeof(cmd));
    enrollRequested = true;
    enrollId = cmd.fingerprint_id;
    currentMode = MODE_ENROLL;
    Serial.printf("📌 Enroll diminta untuk ID: %d\n", enrollId);
  } else {
    Serial.println("⚠️ Data size mismatch!");
  }
}

void setup() {
  Serial.begin(115200);
  mySerial.begin(57600, SERIAL_8N1, 16, 17);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("❌ OLED tidak ditemukan");
    while (true) delay(1000);
  }
  display.clearDisplay(); display.display();

  // RTC
  if (!rtc.begin()) {
    Serial.println("❌ RTC tidak ditemukan!");
    while (true);
  }
  if (rtc.lostPower()) rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  // Fingerprint
  if (finger.verifyPassword()) {
    Serial.println("✅ Sensor fingerprint terdeteksi");
  } else {
    Serial.println("❌ Fingerprint sensor tidak ditemukan!");
    while (true);
  }

  // ESP-NOW
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW gagal init");
    return;
  }
  esp_now_register_recv_cb(onDataReceived);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, esp32uMac, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (!esp_now_is_peer_exist(esp32uMac)) {
    esp_now_add_peer(&peerInfo);
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
    if (p != FINGERPRINT_OK) return;
    p = finger.image2Tz();
    if (p != FINGERPRINT_OK) return;
    p = finger.fingerSearch();
    if (p == FINGERPRINT_OK) {
      DateTime now = rtc.now();
      absensiData.fingerprint_id = finger.fingerID;
      sprintf(absensiData.timestamp, "%04d-%02d-%02d %02d:%02d:%02d",
              now.year(), now.month(), now.day(),
              now.hour(), now.minute(), now.second());

      esp_now_send(esp32uMac, (uint8_t*)&absensiData, sizeof(absensiData));
      Serial.printf("👤 Absen ID %d dikirim ke ESP32U\n", absensiData.fingerprint_id);
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
