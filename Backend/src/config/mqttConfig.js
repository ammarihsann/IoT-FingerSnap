const mqtt = require("mqtt");
const { db, bucket } = require("./firebase");
const cron = require("node-cron");

const MQTT_BROKER =
	"mqtts://20b4045b4af5432c9978e9c8d1f97a15.s1.eu.hivemq.cloud";
const MQTT_USERNAME = "hivemq.webclient.1743687647826";
const MQTT_PASSWORD = "x:WJf80AtIl5UagY$;1>";
const TOPICS = ["/enroll", "/absensi", "/image/raw"];

const mqttClient = mqtt.connect(MQTT_BROKER, {
	username: MQTT_USERNAME,
	password: MQTT_PASSWORD,
});

let lastFingerprintId = null;
let lastAttendanceDocId = null;
let lastAbsensiTime = null; // variabel untuk menyimpan waktu absensi terakhir

mqttClient.on("connect", () => {
	console.log("✅ Terhubung ke broker MQTT");
	mqttClient.subscribe(TOPICS, (err) => {
		if (err) {
			console.error("❌ Gagal subscribe ke topics:", TOPICS);
		} else {
			console.log(`✅ Berhasil subscribe ke topics: ${TOPICS.join(", ")}`);
		}
	});
});

mqttClient.on("error", (err) => {
	console.error("❌ Error MQTT:", err.message);
});

mqttClient.on("message", async (topic, message) => {
	console.log(
		`📥 Pesan diterima di topic ${topic}:`,
		topic === "/image/raw" ? "[binary image]" : message.toString()
	);

	if (topic === "/absensi") {
		try {
			const data = JSON.parse(message.toString());
			const { fingerprint_id, timestamp } = data;
			if (!fingerprint_id || !timestamp) {
				console.error("❌ Data absensi tidak lengkap:", data);
				return;
			}

			const fingerprintIdStr = String(fingerprint_id);
			if (!fingerprintIdStr || fingerprintIdStr === "NaN") {
				console.error("❌ fingerprint_id tidak valid:", fingerprint_id);
				return;
			}

			const [date, time] = timestamp.split(" ");
			const jam = time.split(":")[0];
			const menit = time.split(":")[1] || "00";
			const jamInt = parseInt(jam, 10);
			const menitInt = parseInt(menit, 10);

			// Ambil batas jam dari Firestore settings
			const settingsDoc = await db.collection("settings").doc("absensi").get();
			const batasJam =
				(settingsDoc.exists && settingsDoc.data().batas_jam) || "12:00";
			const [batasJamInt, batasMenitInt] = batasJam.split(":").map(Number);

			if (
				jamInt > batasJamInt ||
				(jamInt === batasJamInt && menitInt >= batasMenitInt)
			) {
				// CHECK-OUT: update dokumen attendance yang sudah ada
				const attendanceSnapshot = await db
					.collection("attendance")
					.where("fingerprint_id", "==", fingerprintIdStr)
					.where("date", "==", date)
					.limit(1)
					.get();

				if (!attendanceSnapshot.empty) {
					const docId = attendanceSnapshot.docs[0].id;
					await db.collection("attendance").doc(docId).update({
						check_out: time,
					});
					console.log(
						`✅ Check-out berhasil diupdate untuk fingerprint_id ${fingerprintIdStr} pada ${date} jam ${time}`
					);
				} else {
					console.warn(
						`⚠ Tidak ditemukan data check-in untuk fingerprint_id ${fingerprintIdStr} pada ${date} saat check-out`
					);
				}

				// Reset buffer
				lastFingerprintId = null;
				lastAttendanceDocId = null;
				lastAbsensiTime = null;
				return;
			} else {
				// CHECK-IN (pagi): proses seperti biasa
				const attendanceData = {
					fingerprint_id: fingerprintIdStr,
					date,
					check_in: time,
					check_out: "",
					photo_url: "",
					created_at: new Date().toISOString(),
				};

				// Simpan absensi dan simpan docId untuk update photo_url nanti
				const docRef = await db.collection("attendance").add(attendanceData);
				lastFingerprintId = fingerprintIdStr;
				lastAttendanceDocId = docRef.id;
				lastAbsensiTime = time; // simpan jam absensi terakhir

				console.log(
					"✅ Data absensi berhasil disimpan:",
					attendanceData,
					"DocID:",
					docRef.id
				);
			}
		} catch (err) {
			console.error("❌ Gagal memproses pesan MQTT absensi:", err.message);
		}
	}

	if (topic === "/image/raw") {
		try {
			if (!lastFingerprintId || !lastAttendanceDocId || !lastAbsensiTime) {
				console.warn(
					"⚠ Gambar diterima tapi belum ada fingerprint_id, attendance docId, atau waktu absensi!"
				);
				return;
			}

			// Ambil batas jam dari Firestore settings
			const settingsDoc = await db.collection("settings").doc("absensi").get();
			const batasJam =
				(settingsDoc.exists && settingsDoc.data().batas_jam) || "12:00";
			const [batasJamInt, batasMenitInt] = batasJam.split(":").map(Number);

			// CEK JAM: hanya simpan gambar jika jam <= batas jam
			const jam = lastAbsensiTime.split(":")[0];
			const menit = lastAbsensiTime.split(":")[1] || "00";
			const jamInt = parseInt(jam, 10);
			const menitInt = parseInt(menit, 10);

			if (
				jamInt > batasJamInt ||
				(jamInt === batasJamInt && menitInt >= batasMenitInt)
			) {
				console.log(
					`⏰ Gambar TIDAK disimpan karena waktu absensi (${lastAbsensiTime}) di atas jam batas ${batasJam}.`
				);
				// Reset buffer agar tidak tertukar
				lastFingerprintId = null;
				lastAttendanceDocId = null;
				lastAbsensiTime = null;
				return;
			}

			// Nama file di bucket
			const fileName = `images/fp_${lastFingerprintId}_${Date.now()}.jpg`;
			const file = bucket.file(fileName);

			// Upload gambar ke bucket
			await file.save(message, {
				metadata: { contentType: "image/jpeg" },
			});

			// Dapatkan URL publik (atau signed URL jika perlu)
			const [url] = await file.getSignedUrl({
				action: "read",
				expires: Date.now() + 7 * 24 * 60 * 60 * 1000, // 7 hari
			});

			// Update attendance dengan photo_url
			await db.collection("attendance").doc(lastAttendanceDocId).update({
				photo_url: url,
			});

			console.log(
				`📸 Gambar fingerprint ${lastFingerprintId} disimpan ke bucket sebagai ${fileName} dan URL disimpan ke attendance.`
			);

			// Reset agar tidak tertukar
			lastFingerprintId = null;
			lastAttendanceDocId = null;
			lastAbsensiTime = null;
		} catch (err) {
			console.error("❌ Gagal menyimpan gambar dari MQTT:", err.message);
		}
	}

	// Handler khusus untuk /enroll bisa ditambahkan di sini jika diperlukan

	// Handler untuk sinkronisasi waktu RTC ESP32
	if (topic === "/time") {
		console.log("⏰ Pesan sinkronisasi waktu diterima:", message.toString());
		// Tidak perlu proses di backend, hanya log
	}
});

// --- Tambahkan scheduler untuk sinkronisasi waktu RTC ESP32 setiap hari jam 4 pagi ---
function syncRtcToEsp() {
	const now = new Date();

	// Tambah 7 jam (25200000 ms) ke waktu UTC
	const wibDate = new Date(now.getTime() + 7 * 60 * 60 * 1000);

	const pad = (n) => String(n).padStart(2, "0");
	const year = wibDate.getUTCFullYear();
	const month = pad(wibDate.getUTCMonth() + 1);
	const day = pad(wibDate.getUTCDate());
	const hour = pad(wibDate.getUTCHours());
	const minute = pad(wibDate.getUTCMinutes());
	const second = pad(wibDate.getUTCSeconds());

	const timestamp = `${year}-${month}-${day} ${hour}:${minute}:${second}`;
	const payload = JSON.stringify({ timestamp });

	console.log("🕐 UTC:", now.toISOString());
	console.log("🕐 WIB (offset):", timestamp);

	mqttClient.publish("/time", payload, (err) => {
		if (err) {
			console.error("❌ Gagal publish RTC sync ke MQTT:", err.message);
		} else {
			console.log("✅ RTC sync dikirim ke ESP (WIB):", payload);
		}
	});
}

// Jalankan setiap hari jam 4 pagi (Asia/Jakarta)
cron.schedule("0 4 * * *", syncRtcToEsp, { timezone: "Asia/Jakarta" });

// Untuk testing manual, bisa panggil syncRtcToEsp();
// syncRtcToEsp();

module.exports = mqttClient;
