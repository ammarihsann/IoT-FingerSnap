const express = require("express");
const router = express.Router();
const mqttClient = require("../config/mqttConfig");
const { db, bucket } = require("../config/firebase");

router.post("/publish", (req, res) => {
	const { fingerprint_id } = req.body;

	console.log("Request diterima di /publish:", req.body);

	if (!fingerprint_id || isNaN(fingerprint_id)) {
		return res
			.status(400)
			.json({ success: false, message: "Fingerprint ID tidak valid." });
	}

	const payload = JSON.stringify({ fingerprint_id });
	const topic = "/enroll";

	mqttClient.publish(topic, payload, (err) => {
		if (err) {
			console.error("❌ Gagal publish ke MQTT:", err.message);
			return res
				.status(500)
				.json({ success: false, message: "Gagal publish ke MQTT." });
		}

		console.log(
			`✅ Fingerprint ID ${fingerprint_id} berhasil dipublish ke MQTT`
		);
		res.json({
			success: true,
			message: `Fingerprint ID ${fingerprint_id} berhasil dipublish ke MQTT`,
		});
	});
});

router.get("/verify", (req, res) => {
	const { fingerprint_id } = req.query;

	if (!fingerprint_id || isNaN(fingerprint_id)) {
		return res
			.status(400)
			.json({ success: false, message: "Fingerprint ID tidak valid." });
	}

	// Simulasi verifikasi MQTT
	const isVerified = Math.random() > 0.5; // Simulasi sukses/gagal

	if (isVerified) {
		res.json({
			success: true,
			message: `Fingerprint ID ${fingerprint_id} berhasil diverifikasi.`,
		});
	} else {
		res
			.status(500)
			.json({ success: false, message: "Gagal memverifikasi koneksi MQTT." });
	}
});

router.get("/check-fingerprint/:fingerprint_id", async (req, res) => {
	const { fingerprint_id } = req.params;

	try {
		const userSnapshot = await db
			.collection("users")
			.where("fingerprint_id", "==", fingerprint_id)
			.get();

		if (!userSnapshot.empty) {
			return res.status(200).json({
				success: true,
				message: "Fingerprint ID sudah terdaftar.",
			});
		}

		res.status(404).json({
			success: false,
			message: "Fingerprint ID belum terdaftar.",
		});
	} catch (error) {
		console.error("Error checking fingerprint ID:", error);
		res.status(500).json({
			success: false,
			message: "Terjadi kesalahan saat memeriksa fingerprint ID.",
		});
	}
});

router.post("/sync-rtc", (req, res) => {
	const now = new Date();
	const wibDate = new Date(now.getTime() + 7 * 60 * 60 * 1000);

	const pad = (n) => n.toString().padStart(2, "0");
	const year = wibDate.getUTCFullYear();
	const month = pad(wibDate.getUTCMonth() + 1);
	const day = pad(wibDate.getUTCDate());
	const hour = pad(wibDate.getUTCHours());
	const minute = pad(wibDate.getUTCMinutes());
	const second = pad(wibDate.getUTCSeconds());

	const timestamp = `${year}-${month}-${day} ${hour}:${minute}:${second}`;
	const payload = JSON.stringify({ timestamp });

	mqttClient.publish("/time", payload, (err) => {
		if (err) {
			console.error("❌ Gagal publish RTC sync ke MQTT:", err.message);
			return res
				.status(500)
				.json({ success: false, message: "Gagal publish RTC sync ke MQTT." });
		}
		console.log("✅ RTC sync manual dikirim ke ESP:", payload);
		res.json({ success: true, message: "RTC ESP berhasil disinkronkan." });
	});
});

module.exports = router;
