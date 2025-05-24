# ESP32 Fingerprint Attendance System (ESP-NOW + MQTT)

Proyek ini merupakan sistem absensi berbasis sidik jari menggunakan dua buah ESP32. ESP32 di basement membaca ID sidik jari dan waktu dari RTC, lalu mengirimkan data ke ESP32 lantai 1 menggunakan protokol **ESP-NOW**. ESP32 lantai 1 kemudian mengirimkan data ke server menggunakan protokol **MQTT** dengan koneksi TLS (port 8883).

## 📦 Fitur Utama

- 🔒 Enkripsi MQTT (TLS) ke broker HiveMQ Cloud.
- 📡 Komunikasi antar ESP32 menggunakan ESP-NOW (tanpa WiFi).
- 👆 Pembacaan sidik jari menggunakan sensor **FPM10A**.
- ⏰ Timestamp dari RTC.
- 🧾 Pengiriman data absensi (ID dan waktu) ke MQTT topic `/absensi`.
- 🧬 Perintah remote untuk **enroll** dan **hapus** sidik jari via MQTT topics `/enroll` dan `/hapus`.
- 🖥️ Komunikasi antar ESP menggunakan **Serial2**.

## 🧰 Teknologi & Library

- Platform: **ESP32**
- Protokol: **ESP-NOW**, **MQTT over TLS**
- Cloud MQTT Broker: **HiveMQ Cloud**
- Sensor: **Fingerprint FPM10A**
- Waktu: **RTC + Timestamp**
- Library:
  - `WiFi.h`, `WiFiClientSecure.h`
  - `PubSubClient`
  - `ArduinoJson`

## 📁 Struktur Perangkat

### Basement (ESP32 #1):
- Sensor sidik jari FPM10A (UART)
- RTC Module DS3231
- OLED Display 
- Mengirim `ABSEN:<id>|<timestamp>` ke lantai 1 via ESP-NOW

### Lantai 1 (ESP32 #2):
- Menerima data ESP-NOW dari basement
- Kirim ke MQTT broker:
  - `topic: /absensi`
  - format payload JSON:
    ```json
    {
      "fingerprint_id": 12,
      "timestamp": "2025-05-24 10:30:45"
    }
    ```
- Menerima perintah MQTT:
  - `ENROLL:<id>` dari topic `/enroll`
  - `HAPUS:<id>` dari topic `/hapus`
  - Kirim ke ESP32 basement via `Serial2`
