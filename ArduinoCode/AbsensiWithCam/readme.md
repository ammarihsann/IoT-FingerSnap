# 📡 Sistem Absensi IoT Berbasis ESP32 dengan Fingerprint, Kamera, dan MQTT

Proyek ini adalah sistem absensi terdistribusi berbasis ESP32 yang mengintegrasikan autentikasi sidik jari, pengambilan foto otomatis, komunikasi nirkabel menggunakan ESP-NOW, serta pengiriman data ke server melalui MQTT. Sistem ini dirancang untuk digunakan di lingkungan seperti sekolah, kantor, atau laboratorium dengan kebutuhan pencatatan kehadiran digital secara real-time dan berbasis cloud.

---

## 🔧 Komponen Sistem

### 1. 🧠 ESP32 Basement
- Membaca sidik jari menggunakan sensor FPM10A
- Menyimpan dan membandingkan ID fingerprint
- Menampilkan status di OLED
- Menyimpan waktu dengan RTC
- Mengirim data absensi ke ESP32U via ESP-NOW
- Mengirim trigger pengambilan gambar ke ESP32-CAM

### 2. 🔁 ESP32U (Lantai 1)
- Bertindak sebagai bridge antara ESP32 Basement, ESP32-CAM, dan ESP32 Lantai 1
- Menerima data absensi dan gambar dari ESP-NOW
- Mengirim metadata dan gambar via Serial2 ke ESP32 Lantai 1

### 3. 📸 ESP32-CAM
- Mengambil gambar saat ada absensi berhasil
- Mengirim gambar dalam format chunk melalui ESP-NOW ke ESP32U

### 4. 🌐 ESP32 Lantai 1 (MQTT Client)
- Terhubung ke internet via WiFi
- Menerima metadata dan gambar dari ESP32U
- Mengirim data ke broker MQTT (HiveMQ Cloud)

---

## 🔄 Alur Data

```plaintext
[Fingerprint] --> [ESP32 Basement]
     |
     |--[ESP-NOW]--> [ESP32U] --> [Serial2] --> [ESP32 Lantai 1] --> [MQTT Broker]
     |
     `--[ESP-NOW Trigger]--> [ESP32-CAM] --> [ESP-NOW] --> [ESP32U]
🗂 Struktur Proyek
/
├── esp32_basement.ino        → Kode untuk fingerprint + RTC + OLED
├── esp32u_bridge.ino         → Kode untuk relay ESP-NOW ↔ Serial
├── esp32cam_sender.ino       → Kode untuk kamera & chunk sender
├── esp32_lantai1_mqtt.ino    → Kode untuk koneksi MQTT + Serial2
├── python_receiver.py        → (Opsional) Untuk debugging MQTT
└── README.md

📡 Topik MQTT
Topik MQTT	Fungsi
/absensi	Fingerprint ID dan timestamp absensi
/gambar-absen	JSON metadata + gambar base64 hasil absensi
/perintah	Perintah dari server (ENROLL, HAPUS, SETTIME)
/time	Sinkronisasi waktu dari server ke RTC

💬 Perintah Serial2 ke ESP32U
Gunakan format berikut untuk mengontrol sistem dari ESP32 Lantai 1 ke ESP32U:
ENROLL:<id>           → Daftarkan fingerprint ID
HAPUS:<id>            → Hapus fingerprint ID
SETTIME:<timestamp>   → Set waktu RTC format: YYYY-MM-DD HH:MM:SS
Contoh:
ENROLL:1
HAPUS:3
SETTIME:2025-06-11 10:45:00

🛠 Dependencies Arduino
Pastikan kamu sudah menginstal library berikut pada Arduino IDE:
ArduinoJson
ESP-NOW (sudah bawaan ESP32 board package)
Adafruit SSD1306
RTClib
esp_camera (bawaan ESP32-CAM board)

⚠️ Catatan Teknis
Ukuran gambar dikirim dalam chunk (misal 230 byte per chunk) agar stabil dalam ESP-NOW.

Waktu menggunakan format ISO YYYY-MM-DD HH:MM:SS.

Setiap fingerprint ID juga dikaitkan dengan foto yang diambil saat absensi.

Pastikan MAC address dari semua node ESP32 sudah ditambahkan sebagai peer ESP-NOW.

Gunakan Serial2 (GPIO 16 dan 17) agar tidak mengganggu debug Serial utama.

📷 Contoh Output
📥 Absensi: ID=3, Time=2025-06-11 10:47:31
🖼 Metadata diterima: ID=3, Size=4920, Chunks=22
📦 Chunk [1/22] diterima (230 bytes)
...
✅ Semua chunk diterima.
📤 Gambar mentah dikirim ke lantai 1
