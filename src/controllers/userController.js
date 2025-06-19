const userService = require("../services/userService");
const mqttClient = require("../config/mqttConfig");
const { db, auth } = require("../config/firebase");

exports.getAllUsers = async (req, res) => {
	const snapshot = await db.collection("users").get();
	const users = [];
	snapshot.forEach((doc) => {
		users.push({ uid: doc.id, ...doc.data() }); // Pastikan uid = doc.id
	});
	res.json({ data: users });
};

exports.updateUser = async (req, res) => {
	try {
		const { id } = req.params;
		const { nama, fingerprint_id, role } = req.body;

		if (!nama || !fingerprint_id || !role) {
			return res.status(400).json({
				success: false,
				message: "nama, fingerprint_id, dan role harus diisi",
			});
		}

		const updatedUser = await userService.updateUser(id, {
			nama,
			fingerprint_id,
			role,
		});

		res.status(200).json({
			success: true,
			message: "User updated successfully",
			data: updatedUser,
		});
	} catch (error) {
		res.status(500).json({ success: false, message: error.message });
	}
};

exports.deleteUser = async (req, res) => {
	try {
		const { id } = req.params;

		// Verifikasi ID
		if (!id) {
			return res.status(400).json({
				success: false,
				message: "ID pengguna tidak valid",
			});
		}

		// Cek apakah user ada sebelum dihapus
		const userRef = db.collection("users").doc(id);
		const doc = await userRef.get();

		if (!doc.exists) {
			return res.status(404).json({
				success: false,
				message: "Pengguna tidak ditemukan",
			});
		}

		// Ambil fingerprint_id sebelum hapus
		const userData = doc.data();
		const fingerprint_id = userData.fingerprint_id;

		// Hapus dari Firestore
		await userRef.delete();

		// Publish ke MQTT topic /hapus jika fingerprint_id ada
		if (fingerprint_id) {
			const payload = JSON.stringify({ fingerprint_id });
			mqttClient.publish("/hapus", payload, (err) => {
				if (err) {
					console.error("❌ Gagal publish ke MQTT:", err.message);
				} else {
					console.log(
						`✅ fingerprint_id ${fingerprint_id} dipublish ke /hapus`
					);
				}
			});
		}

		// Verifikasi penghapusan
		const verifyDoc = await userRef.get();
		if (verifyDoc.exists) {
			return res.status(500).json({
				success: false,
				message:
					"Gagal menghapus pengguna - dokumen masih ada setelah penghapusan",
			});
		}

		res.status(200).json({
			success: true,
			message: "User deleted successfully & MQTT notified",
			fingerprint_id,
		});
	} catch (error) {
		console.error("Error deleting user:", error);
		res.status(500).json({
			success: false,
			message: error.message,
		});
	}
};

exports.updateUsername = async (req, res) => {
	try {
		const uid = req.user.uid;
		const { nama } = req.body;

		if (!nama) {
			return res.status(400).json({
				success: false,
				message: "Nama harus diisi",
			});
		}

		await db.collection("users").doc(uid).update({ nama });
		await auth.updateUser(uid, { displayName: nama });

		res.status(200).json({
			success: true,
			message: "Nama berhasil diperbarui",
		});
	} catch (error) {
		res.status(500).json({
			success: false,
			message: "Gagal update nama",
			error: error.message,
		});
	}
};

exports.updatePassword = async (req, res) => {
	try {
		const uid = req.user.uid;
		const { oldPassword, newPassword, confirmPassword } = req.body;

		if (!oldPassword || !newPassword || !confirmPassword) {
			return res.status(400).json({
				success: false,
				message: "Semua field password harus diisi",
			});
		}

		if (newPassword !== confirmPassword) {
			return res.status(400).json({
				success: false,
				message: "Konfirmasi password baru tidak cocok",
			});
		}

		// Ambil email user dari Firestore
		const userDoc = await db.collection("users").doc(uid).get();
		if (!userDoc.exists) {
			return res.status(404).json({
				success: false,
				message: "User tidak ditemukan",
			});
		}
		const userData = userDoc.data();
		const email = userData.email;

		// Verifikasi password lama menggunakan Firebase Auth REST API
		const firebaseApiKey = process.env.FIREBASE_API_KEY;
		const axios = require("axios");
		try {
			await axios.post(
				`https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=${firebaseApiKey}`,
				{
					email,
					password: oldPassword,
					returnSecureToken: false,
				}
			);
		} catch (err) {
			return res.status(400).json({
				success: false,
				message: "Password lama salah",
			});
		}

		// Update password di Firebase Auth
		await auth.updateUser(uid, { password: newPassword });

		res.status(200).json({
			success: true,
			message: "Password berhasil diperbarui",
		});
	} catch (error) {
		res.status(500).json({
			success: false,
			message: "Gagal update password",
			error: error.message,
		});
	}
};
