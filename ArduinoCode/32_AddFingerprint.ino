#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>

HardwareSerial mySerial(2); // Gunakan Serial2 pada ESP32
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);
uint8_t id = 0; // ID sidik jari yang akan didaftarkan

void setup() {
    Serial.begin(115200);
    mySerial.begin(57600, SERIAL_8N1, 16, 17); // RX = GPIO16, TX = GPIO17

    Serial.println("🔄 Inisialisasi sensor fingerprint...");

    finger.begin(57600);
    if (finger.verifyPassword()) {
        Serial.println("✅ Sensor fingerprint terdeteksi!");
    } else {
        Serial.println("❌ Sensor fingerprint tidak ditemukan. Periksa koneksi!");
        while (1);
    }
}

void loop() {
    Serial.println("\nMasukkan ID untuk sidik jari baru (1-127): ");

    while (Serial.available()) Serial.read(); // Bersihkan buffer Serial
    while (Serial.available() == 0); // Tunggu input dari Serial Monitor

    id = Serial.parseInt();
    if (id <= 0 || id > 127) {
        Serial.println("⚠️ ID tidak valid! Masukkan angka antara 1-127.");
        return;
    }

    if (checkIfIDExists(id)) {
        Serial.println("⚠️ ID sudah terdaftar! Coba gunakan ID lain.");
        return;
    }

    Serial.print("🔄 Mendaftarkan ID: ");
    Serial.println(id);

    if (enrollFingerprint(id)) {
        Serial.println("✅ Sidik jari berhasil didaftarkan!");
    } else {
        Serial.println("❌ Gagal mendaftarkan sidik jari. Coba lagi.");
    }

    delay(2000);
}

// Fungsi untuk pendaftaran sidik jari
bool enrollFingerprint(uint8_t id) {
    int p;

    Serial.println("\n🖐️ Letakkan jari Anda di sensor...");
    while ((p = finger.getImage()) != FINGERPRINT_OK) {
        if (p == FINGERPRINT_NOFINGER) {
            Serial.print(".");
        } else {
            Serial.println("❌ Gagal membaca gambar, coba lagi.");
            return false;
        }
        delay(100);
    }

    Serial.println("\n✅ Gambar pertama berhasil diambil! Memproses...");
    p = finger.image2Tz(1);
    if (p != FINGERPRINT_OK) {
        Serial.println("❌ Gagal mengubah gambar ke template.");
        return false;
    }

    Serial.println("\n👉 Angkat jari dan letakkan kembali...");
    delay(2000);

    while (finger.getImage() != FINGERPRINT_NOFINGER) {
        delay(100);
    }

    Serial.println("\n🖐️ Letakkan kembali jari yang sama...");
    while ((p = finger.getImage()) != FINGERPRINT_OK) {
        if (p == FINGERPRINT_NOFINGER) {
            Serial.print(".");
        } else {
            Serial.println("❌ Gagal membaca gambar, coba lagi.");
            return false;
        }
        delay(100);
    }

    Serial.println("\n✅ Gambar kedua berhasil diambil! Memproses...");
    p = finger.image2Tz(2);
    if (p != FINGERPRINT_OK) {
        Serial.println("❌ Gagal mengubah gambar ke template.");
        return false;
    }

    Serial.println("\n🔄 Menggabungkan template...");
    p = finger.createModel();
    if (p != FINGERPRINT_OK) {
        Serial.println("❌ Gagal menggabungkan template.");
        return false;
    }

    Serial.println("\n💾 Menyimpan sidik jari ke ID: " + String(id));
    p = finger.storeModel(id);
    if (p != FINGERPRINT_OK) {
        Serial.println("❌ Gagal menyimpan sidik jari.");
        return false;
    }

    return true;
}

// Fungsi untuk mengecek apakah ID sudah terdaftar
bool checkIfIDExists(uint8_t id) {
    uint8_t p = finger.loadModel(id);
    return (p == FINGERPRINT_OK);
}
