const { db } = require("../config/firebase");

exports.getRecentActivities = async (req, res) => {
	try {
		const activities = [];

		// Ambil tanggal hari ini (format YYYY-MM-DD)
		const today = new Date();
		const pad = (n) => String(n).padStart(2, "0");
		const year = today.getFullYear();
		const month = pad(today.getMonth() + 1);
		const day = pad(today.getDate());
		const todayStr = `${year}-${month}-${day}`;

		// Ambil data absensi terbaru
		const attendanceSnapshot = await db
			.collection("attendance")
			.orderBy("date", "desc")
			.get();

		attendanceSnapshot.forEach((doc) => {
			const data = doc.data();
			// Hanya tampilkan absensi hari ini
			if (data.date === todayStr) {
				activities.push({
					type: "attendance",
					icon: "bi-person-check",
					title: "Absensi",
					description: `${data.nama || "Pegawai"} telah ${
						data.status || "hadir"
					}`,
					timestamp: data.date || new Date().toISOString(),
				});
			}
		});

		// Ambil data pegawai baru hari ini
		const usersSnapshot = await db
			.collection("users")
			.orderBy("created_at", "desc")
			.get();

		usersSnapshot.forEach((doc) => {
			const data = doc.data();
			// Cek apakah created_at hari ini (format ISO string)
			if (data.created_at && data.created_at.startsWith(todayStr)) {
				activities.push({
					type: "user",
					icon: "bi-person-plus",
					title: "Pegawai Baru",
					description: `${data.nama || "Pegawai"} ditambahkan sebagai ${
						data.role || "karyawan"
					}`,
					timestamp: data.created_at,
				});
			}
		});

		// Urutkan aktivitas berdasarkan waktu terbaru
		activities.sort((a, b) => new Date(b.timestamp) - new Date(a.timestamp));

		res.json({ success: true, data: activities });
	} catch (error) {
		console.error("Error fetching recent activities:", error);
		res
			.status(500)
			.json({ success: false, message: "Gagal mengambil aktivitas terbaru" });
	}
};

exports.getAllActivities = async (req, res) => {
	try {
		const activities = [];

		// Ambil data absensi
		const attendanceSnapshot = await db
			.collection("attendance")
			.orderBy("date", "desc")
			.get(); // Tidak ada limit untuk mengambil semua data
		attendanceSnapshot.forEach((doc) => {
			const data = doc.data();
			activities.push({
				type: "attendance",
				icon: "bi-person-check",
				title: "Absensi",
				description: `${data.nama || "Pegawai"} telah ${
					data.status || "hadir"
				}`,
				timestamp: data.date || new Date().toISOString(),
			});
		});

		// Ambil data pegawai baru
		const usersSnapshot = await db
			.collection("users")
			.orderBy("created_at", "desc")
			.get(); // Tidak ada limit untuk mengambil semua data
		usersSnapshot.forEach((doc) => {
			const data = doc.data();
			activities.push({
				type: "user",
				icon: "bi-person-plus",
				title: "Pegawai Baru",
				description: `${data.nama || "Pegawai"} ditambahkan sebagai ${
					data.role || "karyawan"
				}`,
				timestamp: data.created_at || new Date().toISOString(),
			});
		});

		// Urutkan aktivitas berdasarkan waktu terbaru
		activities.sort((a, b) => new Date(b.timestamp) - new Date(a.timestamp));

		res.json({ success: true, data: activities });
	} catch (error) {
		console.error("Error fetching all activities:", error);
		res
			.status(500)
			.json({ success: false, message: "Gagal mengambil aktivitas" });
	}
};

exports.resetActivities = async (req, res) => {
	try {
		const snapshot = await db.collection("activities").get();
		const batch = db.batch();

		snapshot.forEach((doc) => batch.delete(doc.ref));
		await batch.commit();

		res.json({ success: true, message: "Semua aktivitas berhasil dihapus" });
	} catch (error) {
		console.error("Error resetting activities:", error);
		res
			.status(500)
			.json({ success: false, message: "Gagal menghapus aktivitas" });
	}
};
