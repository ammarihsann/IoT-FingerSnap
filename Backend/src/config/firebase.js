const admin = require("firebase-admin");
const serviceAccount = require("./serviceAccountKey.json");

admin.initializeApp({
	credential: admin.credential.cert(serviceAccount),
	storageBucket: "pt_bit", // Pastikan nama bucket sesuai dengan konfigurasi Anda
});

const db = admin.firestore();
const bucket = admin.storage().bucket();
const auth = admin.auth();

// Hapus pengaturan databaseId jika menggunakan database default
module.exports = { auth, admin, db, bucket };
