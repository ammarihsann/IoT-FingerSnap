const express = require("express");
const router = express.Router();
const absensiController = require("../controllers/absensiController");
const authMiddleware = require("../middlewares/authMiddleware");
const multer = require("multer");
const upload = multer({ storage: multer.memoryStorage() });

let latestFingerprintId = null; // Buffer untuk menyimpan fingerprint_id sementara

router.post("/enroll", (req, res) => {
	const { fingerprint_id } = req.body;

	if (!fingerprint_id) {
		return res.status(400).json({ message: "❌ fingerprint_id missing" });
	}

	latestFingerprintId = fingerprint_id; // Simpan di buffer sementara
	console.log(
		`📥 fingerprint_id ${fingerprint_id} disiapkan untuk dikirim ke ESP32U`
	);
	res.json({ message: "✅ Fingerprint siap dikirim ke ESP32U" });
});

router.get("/perintah", (req, res) => {
	if (latestFingerprintId) {
		const response = { fingerprint_id: latestFingerprintId };
		latestFingerprintId = null; // Hapus setelah dikirim
		return res.json(response);
	}
	res.json({}); // Tidak ada data untuk dikirim
});

router.get(
	"/attendance-by-user",
	authMiddleware,
	absensiController.getAttendanceByLoggedInUser
);

module.exports = router;

router.post("/register", async (req, res) => {
	const { nama, fingerprint_id } = req.body;

	if (!nama || !fingerprint_id) {
		return res.status(400).json({ message: "❌ Data tidak valid!" });
	}

	try {
		const fingerprintId = parseInt(fingerprint_id, 10);
		if (isNaN(fingerprintId) || fingerprintId < 1 || fingerprintId > 127) {
			return res.status(400).json({
				message: "❌ Fingerprint ID harus angka antara 1-127",
			});
		}

		const timestamp = new Date().toISOString();

		// Simpan data ke Firestore
		const newUser = {
			nama,
			fingerprint_id: fingerprintId,
			timestamp,
		};

		await db.collection("employees").add(newUser);
		res.json({ message: "✅ Data berhasil disimpan ke Firestore" });
	} catch (error) {
		res.status(500).json({ message: `❌ Error: ${error.message}` });
	}
});

// Endpoint untuk menyimpan data absensi
router.post(
	"/attendance",
	authMiddleware,
	upload.single("photo"), // Middleware untuk menangani file dengan field "photo"
	absensiController.createAttendance
);

// GET /api/absensi → get list absensi user yang login
router.get("/", authMiddleware, absensiController.getAbsensiUser);

// POST /api/fingerprint/register → admin mendaftarkan fingerprint user
router.post("/register-fingerprint", absensiController.registerFingerprint);

router.get("/rekap", absensiController.rekapAbsensi);

// Endpoint untuk mendapatkan data kehadiran berdasarkan tanggal
router.get(
	"/attendance",
	authMiddleware,
	absensiController.getAttendanceByDate
);

module.exports = router;
