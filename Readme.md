<p align="center">
  <img src="Web_Admin/public/assets/Scan_in_full(putih).png" alt="Scan In Logo" width="350"/>
</p>

# 🚀 Scan In

Sistem absensi digital berbasis IoT yang modern, praktis, dan real-time. Menggabungkan ESP32, fingerprint, kamera, backend Node.js, dan web admin responsif untuk pengalaman absensi yang seamless.

---

## ✨ Fitur Utama

- 👆 Absensi sidik jari & foto otomatis
- ☁️ Data tersimpan di cloud (Firestore & Google Cloud Storage)
- ⏰ Pengaturan jam check-in/out dinamis via web
- 📊 Dashboard monitoring absensi, aktivitas, & perangkat
- 🔗 Komunikasi perangkat via MQTT

---

## 🗂️ Struktur Proyek

```
/
├── 🟦 ArduinoCode/   → Kode ESP32 (absensi, kamera, bridge)
├── 🟩 Backend/       → Backend Node.js + Express + Firebase
└── 🟨 Web_Admin/     → Web admin (dashboard, pengaturan, rekap)
```

---

## 🟩 Backend (Node.js + Express + Firebase)

- REST API untuk autentikasi, user, absensi, jadwal, aktivitas, pengaturan jam absensi
- Integrasi MQTT: menerima data absensi & gambar dari perangkat IoT
- Penyimpanan data absensi di Firestore, gambar di Google Cloud Storage
- Pengaturan jam check-in/out dinamis
- Sinkronisasi waktu RTC ke perangkat via MQTT
- Load test & stress test dengan k6 (lihat folder TESTING)
- File rahasia (serviceAccountKey.json, .env) tidak disertakan di repo
- Struktur utama:
  - `src/controllers/` – logika API (absensi, user, auth, jadwal, aktivitas)
  - `src/routes/` – endpoint REST API & MQTT
  - `src/services/` – akses data & logika bisnis
  - `src/config/` – konfigurasi Firebase, MQTT, dsb

---

## 🟨 Web Admin

- Dashboard real-time: monitoring absensi, aktivitas, status perangkat
- Kelola pegawai, jadwal, pengaturan jam absensi, rekap kehadiran
- Form pengaturan jam check-in/out (langsung update ke backend)
- Komunikasi ke backend via Fetch (AJAX)
- Tampilan responsif, berbasis HTML, CSS, JavaScript, Bootstrap
- Struktur utama:
  - `public/admin-dashboard/` – halaman dashboard, absensi, jadwal, pegawai, pengaturan
  - `public/assets/` – aset gambar/logo

---
