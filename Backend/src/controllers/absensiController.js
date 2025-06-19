const absensiService = require("../services/absensiService");
const { db, bucket } = require("../config/firebase");
const { v4: uuidv4 } = require("uuid");
const ExcelJS = require("exceljs");

// Mapping role → prefix kode
const rolePrefixMap = {
	resepsionis: "RS",
	satpam: "SP",
	"cleaning service": "CS",
	security: "SC",
};

// Normalisasi jam ke HH:mm:ss
function normalizeTimeStr(timeStr) {
	if (!timeStr) return "";
	const parts = timeStr.split(":");
	if (parts.length === 3) return timeStr;
	if (parts.length === 2) return timeStr + ":00";
	if (parts.length === 1) return timeStr + ":00:00";
	return "";
}

// Status kehadiran dinamis
function getStatusKehadiran(checkIn, jamMasuk, tanggal) {
	if (!checkIn || !jamMasuk) return "tidak hadir";

	const checkInNorm = normalizeTimeStr(checkIn);
	const jamMasukNorm = normalizeTimeStr(jamMasuk);

	const checkInTime = new Date(`${tanggal}T${checkInNorm}`);
	const scheduleTime = new Date(`${tanggal}T${jamMasukNorm}`);

	const hadirThreshold = new Date(scheduleTime);
	hadirThreshold.setMinutes(hadirThreshold.getMinutes() + 5);

	const terlambatThreshold = new Date(scheduleTime);
	terlambatThreshold.setMinutes(terlambatThreshold.getMinutes() + 15);

	if (checkInTime <= hadirThreshold) {
		return "hadir";
	} else if (
		checkInTime > hadirThreshold &&
		checkInTime <= terlambatThreshold
	) {
		return "terlambat";
	} else {
		return "tidak hadir";
	}
}

// GET absensi user login
exports.getAttendanceByLoggedInUser = async (req, res) => {
	try {
		const { nama, fingerprint_id } = req.user;

		if (!nama || !fingerprint_id) {
			return res.status(400).json({
				success: false,
				message: "Nama atau fingerprint ID tidak ditemukan dalam token.",
			});
		}

		const attendanceSnapshot = await db
			.collection("attendance")
			.where("fingerprint_id", "==", fingerprint_id)
			.orderBy("date", "desc")
			.get();

		if (attendanceSnapshot.empty) {
			return res.status(200).json({
				success: true,
				data: [],
				message: "Tidak ada data absensi untuk pengguna ini.",
			});
		}

		const scheduleSnapshot = await db.collection("schedules").get();
		const schedules = {};
		scheduleSnapshot.forEach((doc) => {
			const schedule = doc.data();
			schedules[schedule.hari] = schedule;
		});

		const attendanceData = attendanceSnapshot.docs.map((doc) => {
			const data = doc.data();
			const dayName = new Date(data.date).toLocaleDateString("id-ID", {
				weekday: "long",
			});
			const schedule = schedules[dayName];
			const jamMasuk = schedule ? schedule.jam_masuk : null;
			const status = getStatusKehadiran(data.check_in, jamMasuk, data.date);

			return {
				id: doc.id,
				...data,
				status,
			};
		});

		res.status(200).json({
			success: true,
			data: attendanceData,
		});
	} catch (error) {
		console.error("Error fetching attendance data for logged-in user:", error);
		res.status(500).json({
			success: false,
			message: "Terjadi kesalahan saat mengambil data absensi.",
		});
	}
};

const generateSequentialId = async (rolePrefix) => {
	try {
		// Query semua dokumen dengan prefix yang sama
		const snapshot = await db
			.collection("users")
			.where("id_karyawan", ">=", rolePrefix + "-")
			.where("id_karyawan", "<=", rolePrefix + "-99")
			.get();

		if (snapshot.empty) {
			return `${rolePrefix}-01`; // Jika belum ada, mulai dari 01
		}

		// Ambil semua ID dengan prefix ini dan extract nomor urutnya
		const numbers = snapshot.docs.map((doc) => {
			const id = doc.data().id_karyawan;
			const numPart = id.split("-")[1];
			return parseInt(numPart, 10);
		});

		// Cari nomor tertinggi
		const highestNum = Math.max(...numbers);

		// Increment dan format dengan leading zero
		const nextNum = highestNum + 1;
		return `${rolePrefix}-${nextNum.toString().padStart(2, "0")}`;
	} catch (error) {
		console.error("Error generating sequential ID:", error);
		// Fallback ke format default jika gagal
		return `${rolePrefix}-01`;
	}
};

exports.createAttendance = async (req, res) => {
	const { fingerprint_id, date, check_in, check_out } = req.body;
	const file = req.file; // File gambar yang diunggah

	if (!fingerprint_id || !date || !check_in || !check_out) {
		return res.status(400).json({
			success: false,
			message: "Data absensi tidak lengkap.",
		});
	}

	try {
		let photoUrl = null;

		// Jika ada file gambar, unggah ke bucket
		if (file) {
			const fileName = `absensi/${uuidv4()}_${file.originalname}`;
			const fileUpload = bucket.file(fileName);

			await fileUpload.save(file.buffer, {
				metadata: {
					contentType: file.mimetype,
				},
			});

			// Set URL publik untuk gambar
			photoUrl = `https://storage.googleapis.com/${bucket.name}/${fileName}`;
		}

		// Simpan data absensi ke Firestore
		const attendanceData = {
			fingerprint_id,
			date,
			check_in,
			check_out,
			photo_url: photoUrl,
		};

		await db.collection("attendance").add(attendanceData);

		res.status(201).json({
			success: true,
			message: "Data absensi berhasil disimpan.",
			data: attendanceData,
		});
	} catch (error) {
		console.error("Error creating attendance:", error);
		res.status(500).json({
			success: false,
			message: "Terjadi kesalahan saat menyimpan data absensi.",
		});
	}
};

// GET /api/absensi → Ambil daftar absensi berdasarkan user login
exports.getAbsensiUser = async (req, res) => {
	try {
		const id_karyawan = req.user.uid;

		const result = await absensiService.getAbsensiByUser(id_karyawan);
		res.status(200).json({ success: true, data: result });
	} catch (error) {
		res.status(500).json({ success: false, message: error.message });
	}
};

// POST /api/fingerprint/register → Admin register fingerprint
exports.registerFingerprint = async (req, res) => {
	const { nama, fingerprint_id, role } = req.body;

	if (!nama || !fingerprint_id) {
		return res
			.status(400)
			.json({ success: false, message: "❌ Data tidak valid!" });
	}

	try {
		const fingerprintId = String(parseInt(fingerprint_id, 10));
		if (isNaN(fingerprintId) || fingerprintId < 1 || fingerprintId > 127) {
			return res.status(400).json({
				success: false,
				message: "❌ Fingerprint ID harus angka antara 1-127",
			});
		}

		const timestamp = new Date().toISOString();

		// Generate id_karyawan berdasarkan role jika tersedia
		let id_karyawan = "";
		if (role && role.trim() !== "") {
			const prefix = rolePrefixMap[role.toLowerCase()] || "EMP";
			id_karyawan = await generateSequentialId(prefix);
		}

		// Cek user dengan nama yang sama
		const userSnapshot = await db
			.collection("users")
			.where("nama", "==", nama)
			.limit(1)
			.get();

		let userRef, userData;
		if (!userSnapshot.empty) {
			// Update user lama
			userRef = userSnapshot.docs[0].ref;
			userData = userSnapshot.docs[0].data();
			await userRef.update({
				fingerprint_id: fingerprintId,
				role: role || "",
				id_karyawan,
				updated_at: timestamp,
			});
			userData = {
				...userData,
				fingerprint_id: fingerprintId,
				role: role || "",
				id_karyawan,
			};
		} else {
			// Buat user baru
			userData = {
				nama,
				fingerprint_id: fingerprintId,
				role: role || "",
				id_karyawan,
				email: "",
				created_at: timestamp,
			};
			userRef = await db.collection("users").add(userData);
		}

		res.json({
			success: true,
			message: `✅ Data pegawai ${nama} berhasil disimpan/diupdate`,
			data: userData,
		});
	} catch (error) {
		console.error("Error registering employee:", error);
		res
			.status(500)
			.json({ success: false, message: `❌ Error: ${error.message}` });
	}
};

exports.getAttendanceByDate = async (req, res) => {
	try {
		const { date } = req.query;
		const attendanceSnapshot = await db
			.collection("attendance")
			.where("date", "==", date)
			.get();

		const scheduleSnapshot = await db.collection("schedules").get();
		const schedules = {};
		scheduleSnapshot.forEach((doc) => {
			const schedule = doc.data();
			schedules[schedule.hari] = schedule;
		});

		const attendanceData = await Promise.all(
			attendanceSnapshot.docs.map(async (doc) => {
				const data = doc.data();

				// Join ke user
				const userSnapshot = await db
					.collection("users")
					.where("fingerprint_id", "==", data.fingerprint_id)
					.limit(1)
					.get();
				let userData = {};
				if (!userSnapshot.empty) {
					userData = userSnapshot.docs[0].data();
				}

				let status = "tidak hadir";
				const dayName = new Date(data.date).toLocaleDateString("id-ID", {
					weekday: "long",
				});
				const schedule = schedules[dayName];
				const jamMasuk = schedule ? schedule.jam_masuk : null;

				if (data.check_in && jamMasuk) {
					status = getStatusKehadiran(data.check_in, jamMasuk, data.date);
				}

				return {
					id: doc.id,
					...data,
					nama: userData.nama || "",
					id_karyawan: userData.id_karyawan || "",
					role: userData.role || "",
					status,
				};
			})
		);

		res.status(200).json({
			success: true,
			data: attendanceData,
		});
	} catch (error) {
		console.error("Error fetching attendance data:", error);
		res.status(500).json({
			success: false,
			message: "Terjadi kesalahan saat mengambil data kehadiran.",
		});
	}
};

exports.rekapAbsensi = async (req, res) => {
	const { days } = req.query;
	const daysInt = parseInt(days, 10) || 1;
	const now = new Date();
	const startDate = new Date(now);
	startDate.setDate(now.getDate() - daysInt + 1);

	try {
		const snapshot = await db
			.collection("attendance")
			.where("date", ">=", startDate.toISOString().slice(0, 10))
			.orderBy("date", "asc")
			.get();

		const data = [];
		snapshot.forEach((doc) => data.push({ id: doc.id, ...doc.data() }));

		const usersSnapshot = await db.collection("users").get();
		const usersMap = {};
		usersSnapshot.forEach((doc) => {
			const user = doc.data();
			usersMap[user.fingerprint_id] = user;
		});

		const scheduleSnapshot = await db.collection("schedules").get();
		const schedules = {};
		scheduleSnapshot.forEach((doc) => {
			const schedule = doc.data();
			schedules[schedule.hari] = schedule;
		});

		const workbook = new ExcelJS.Workbook();
		const worksheet = workbook.addWorksheet("Rekap Absensi");

		worksheet.columns = [
			{ header: "Tanggal", key: "date", width: 15 },
			{ header: "ID Karyawan", key: "id_karyawan", width: 15 },
			{ header: "Nama", key: "nama", width: 20 },
			{ header: "Role", key: "role", width: 15 },
			{ header: "Fingerprint ID", key: "fingerprint_id", width: 15 },
			{ header: "Check-in", key: "check_in", width: 12 },
			{ header: "Check-out", key: "check_out", width: 12 },
			{ header: "Status", key: "status", width: 12 },
		];

		for (const row of data) {
			const user = usersMap[row.fingerprint_id] || {};

			let status = "";
			if (row.check_in && row.date) {
				const dayName = new Date(row.date).toLocaleDateString("id-ID", {
					weekday: "long",
				});
				const schedule = schedules[dayName];
				const jamMasuk = schedule ? schedule.jam_masuk : null;
				if (jamMasuk) {
					status = getStatusKehadiran(row.check_in, jamMasuk, row.date);
				} else {
					status = "tidak diketahui";
				}
			} else {
				status = "tidak hadir";
			}

			worksheet.addRow({
				date: row.date,
				id_karyawan: user.id_karyawan || "",
				nama: user.nama || "",
				role: user.role || "",
				fingerprint_id: row.fingerprint_id || "",
				check_in: row.check_in || "",
				check_out: row.check_out || "",
				status: status,
			});
		}

		const todayStr = now.toISOString().slice(0, 10).replace(/-/g, "");
		res.setHeader(
			"Content-Type",
			"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"
		);
		res.setHeader(
			"Content-Disposition",
			`attachment; filename=Rekap_Absensi_${todayStr}.xlsx`
		);
		await workbook.xlsx.write(res);
		res.end();
	} catch (err) {
		res
			.status(500)
			.json({ message: "Gagal membuat rekap absensi", error: err.message });
	}
};
