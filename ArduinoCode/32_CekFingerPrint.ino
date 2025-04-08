#include <Adafruit_Fingerprint.h>

// Gunakan HardwareSerial untuk ESP32
#define RX_PIN 16  // GPIO16 (RX) -> TX sensor fingerprint
#define TX_PIN 17  // GPIO17 (TX) -> RX sensor fingerprint

HardwareSerial mySerial(2);  // UART2 untuk ESP32
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

void setup() {
    Serial.begin(115200);
    mySerial.begin(57600, SERIAL_8N1, RX_PIN, TX_PIN);
    
    finger.begin(57600);
    
    if (finger.verifyPassword()) {
        Serial.println("Sensor fingerprint terdeteksi!");
    } else {
        Serial.println("Sensor fingerprint tidak terdeteksi. Periksa koneksi!");
        while (1);
    }
}

void loop() {
    Serial.println("\nLetakkan jari di sensor...");
    while (finger.getImage() != FINGERPRINT_OK);

    if (finger.image2Tz(1) != FINGERPRINT_OK) {
        Serial.println("Gagal mengubah citra ke template!");
        return;
    }

    if (finger.fingerFastSearch() == FINGERPRINT_OK) {
        Serial.print("Sidik jari cocok dengan ID ");
        Serial.println(finger.fingerID);
    } else {
        Serial.println("Sidik jari tidak ditemukan!");
    }

    delay(2000);
}
