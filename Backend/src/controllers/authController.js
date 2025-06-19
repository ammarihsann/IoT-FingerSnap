const { auth, admin, db } = require("../config/firebase");
const axios = require("axios");

// POST /api/auth/register
exports.register = async (req, res) => {
	try {
		const { email, password, nama } = req.body;

		if (!email || !password || !nama) {
			return res.status(400).json({
				success: false,
				message: "Email, password, dan nama harus diisi",
			});
		}

		// Cek apakah user sudah ada di Firebase Auth
		const existing = await auth.getUserByEmail(email).catch(() => null);
		if (existing) {
			return res
				.status(400)
				.json({ success: false, message: "Email sudah terdaftar" });
		}

		// 1. Buat user di Firebase Auth
		const userRecord = await auth.createUser({
			email,
			password,
			displayName: nama,
		});

		// 2. Cek apakah user dengan nama sudah ada di Firestore
		const userSnapshot = await db
			.collection("users")
			.where("nama", "==", nama)
			.limit(1)
			.get();

		let userData;
		if (!userSnapshot.empty) {
			// Jika sudah ada, update data user lama
			const userRef = userSnapshot.docs[0].ref;
			userData = userSnapshot.docs[0].data();
			await userRef.update({
				email,
				uid: userRecord.uid,
				updated_at: new Date().toISOString(),
			});
			userData = { ...userData, email, uid: userRecord.uid };
		} else {
			// Jika belum ada, buat user baru
			userData = {
				uid: userRecord.uid,
				email,
				nama,
				role: "",
				id_karyawan: "",
				fingerprint_id: "",
				created_at: new Date().toISOString(),
			};
			await db.collection("users").doc(userRecord.uid).set(userData);
		}

		res.status(201).json({
			success: true,
			message: "Registrasi berhasil",
			user: userData,
		});
	} catch (error) {
		res.status(500).json({
			success: false,
			message: "Gagal registrasi",
			error: error.message,
		});
	}
};

// Login user
exports.login = async (req, res) => {
	const { email, password } = req.body;

	try {
		// Use Firebase Auth REST API for login
		const firebaseApiKey = process.env.FIREBASE_API_KEY;
		const response = await axios.post(
			`https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=${firebaseApiKey}`,
			{
				email,
				password,
				returnSecureToken: true,
			}
		);

		const { localId: uid, idToken } = response.data;

		// Fetch user data from Firestore
		const userDoc = await db.collection("users").doc(uid).get();
		if (!userDoc.exists) {
			return res.status(404).json({
				success: false,
				message: "User not found in Firestore",
			});
		}

		const userData = userDoc.data();

		res.status(200).json({
			success: true,
			message: "Login successful",
			token: idToken, // Firebase ID Token
			data: {
				uid,
				email,
				nama: userData.nama,
				role: userData.role,
				id_karyawan: userData.id_karyawan,
			},
		});
	} catch (error) {
		const errorMsg = error.response?.data?.error?.message || error.message;
		res.status(401).json({
			success: false,
			message: "Login failed",
			error: errorMsg,
		});
	}
};

// POST /api/auth/logout
exports.logout = async (req, res) => {
	try {
		// Kalau ada sistem penyimpanan token di sisi backend, bisa tambahkan blacklist di sini
		res.status(200).json({
			success: true,
			message: "Logout berhasil (client-side)",
		});
	} catch (error) {
		res.status(500).json({
			success: false,
			message: "Gagal logout",
			error: error.message,
		});
	}
};

exports.forgetPassword = async (req, res) => {
	try {
		const { email } = req.body;
		if (!email) {
			return res.status(400).json({
				success: false,
				message: "Email harus diisi",
			});
		}

		// Gunakan Firebase Auth REST API untuk mengirim email reset password
		const firebaseApiKey = process.env.FIREBASE_API_KEY;
		const url = `https://identitytoolkit.googleapis.com/v1/accounts:sendOobCode?key=${firebaseApiKey}`;
		const axios = require("axios");
		await axios.post(url, {
			requestType: "PASSWORD_RESET",
			email,
		});

		res.status(200).json({
			success: true,
			message: "Email reset password telah dikirim. Silakan cek email Anda.",
		});
	} catch (error) {
		res.status(500).json({
			success: false,
			message: "Gagal mengirim email reset password",
			error: error.message,
		});
	}
};
