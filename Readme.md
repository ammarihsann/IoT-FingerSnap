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

## 🔗 Preview Scan In

![Login Page](https://drive.google.com/uc?export=view&id=1e2_IxdChnWosdi_f-kQN4xp1ufVG_c67)

<p align="center"><i>Halaman login untuk admin Scan In. Masukkan email dan password untuk mengakses dashboard.</i></p>

<!-- Dashboard Utama -->

![Dashboard Utama](https://drive.google.com/uc?export=view&id=1W2MIAe4k8VwBsrnKd1ORWzUc1nRDCV0p)

<p align="center"><i>Dashboard utama aplikasi Scan In: menampilkan statistik pegawai, jadwal, dan status server.</i></p>

<!-- Mengelola Pegawai -->

![Kelola Pegawai](https://drive.google.com/uc?export=view&id=1wTyfkmFAXD6SCgk4qwo4QMwXiTHUBxzW)

<p align="center"><i>Fitur kelola pegawai: tambah, edit, dan hapus data pegawai beserta fingerprint ID.</i></p>

<!-- Mengelola Jadwal -->

![Kelola Jadwal](https://drive.google.com/uc?export=view&id=1AZUX0FlyCfGFFBsvVogeUK1vM61xdiIC)

<p align="center"><i>Fitur kelola jadwal: atur jam masuk dan keluar untuk setiap hari kerja.</i></p>

<!-- Kehadiran Pegawai -->

![Kehadiran Pegawai](https://drive.google.com/uc?export=view&id=18C2JTR9cmukj8-U9jkcd9wVcUNaokr6q)

<p align="center"><i>Rekap kehadiran pegawai: monitoring status hadir/tidak hadir setiap hari.</i></p>
