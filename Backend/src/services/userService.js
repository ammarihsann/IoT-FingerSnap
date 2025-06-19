const { db } = require("../config/firebase");

const roleMapping = {
	resepsionis: "RS",
	satpam: "SP",
	"cleaning service": "CS",
	security: "SC",
};

const generateIdKaryawan = async (roleCode) => {
	const snapshot = await db
		.collection("users")
		.where("id_karyawan", ">=", `${roleCode}-01`)
		.where("id_karyawan", "<=", `${roleCode}-99`)
		.get();

	const count = snapshot.size + 1;
	return `${roleCode}-${count.toString().padStart(2, "0")}`;
};

exports.updateUser = async (id, { nama, fingerprint_id, role }) => {
	const userRef = db.collection("users").doc(id);
	const userDoc = await userRef.get();

	if (!userDoc.exists) {
		throw new Error("User tidak ditemukan");
	}

	const existingUser = userDoc.data();

	let id_karyawan = existingUser.id_karyawan;
	let updateIdKaryawan = false;

	// Jika role berubah, generate ulang id_karyawan
	if (role && role.toLowerCase() !== (existingUser.role || "").toLowerCase()) {
		const roleCode = roleMapping[role.toLowerCase()];
		if (!roleCode) {
			throw new Error("Role tidak valid");
		}

		// Hapus id_karyawan lama (opsional, jika ingin benar-benar kosongkan dulu)
		id_karyawan = "";

		// Generate id_karyawan baru yang unik
		let nextNum = 1;
		let newIdKaryawan;
		let isUnique = false;
		while (!isUnique) {
			newIdKaryawan = `${roleCode}-${nextNum.toString().padStart(2, "0")}`;
			const snapshot = await db
				.collection("users")
				.where("id_karyawan", "==", newIdKaryawan)
				.get();
			if (snapshot.empty) {
				isUnique = true;
			} else {
				nextNum++;
			}
		}
		id_karyawan = newIdKaryawan;
		updateIdKaryawan = true;
	}

	const updatedData = {
		nama,
		fingerprint_id,
		role,
		id_karyawan,
	};

	await userRef.update(updatedData);
	return updatedData;
};

exports.getAllUsers = async () => {
	const snapshot = await db.collection("users").get();
	return snapshot.docs.map((doc) => ({ id: doc.id, ...doc.data() }));
};

exports.deleteUser = async (id) => {
	try {
		// Verifikasi dokumen ada
		const userRef = db.collection("users").doc(id);
		const doc = await userRef.get();

		if (!doc.exists) {
			throw new Error(`User dengan ID ${id} tidak ditemukan`);
		}

		// Hapus dokumen
		await userRef.delete();

		console.log(`User dengan ID ${id} telah dihapus dari Firestore`);
		return true;
	} catch (error) {
		console.error(`Failed to delete user ${id}:`, error);
		throw new Error(`Failed to delete user: ${error.message}`);
	}
};
