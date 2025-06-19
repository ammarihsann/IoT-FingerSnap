const { db } = require("../config/firebase");

// Buat data absensi baru
exports.createAbsensi = async ({
  id_karyawan,
  fingerprint_id,
  schedule_id,
  status,
  date,
  check_in,
  check_out,
  photo_url,
}) => {
  try {
    // Cek apakah karyawan ada di database
    const employeeRef = db.collection("employees").doc(id_karyawan);
    const employeeDoc = await employeeRef.get();

    if (!employeeDoc.exists) {
      throw new Error("Karyawan tidak ditemukan.");
    }

    // Validasi fingerprint_id
    if (employeeDoc.data().fingerprint_id !== fingerprint_id) {
      throw new Error("Fingerprint tidak valid.");
    }

    // Simpan data absensi ke Firestore
    const newAbsensi = {
      id_karyawan,
      fingerprint_id,
      schedule_id,
      status,
      date,
      check_in,
      check_out,
      photo_url,
    };

    const docRef = await db.collection("attendance").add(newAbsensi);
    return { id: docRef.id, ...newAbsensi };
  } catch (error) {
    throw new Error("Gagal menyimpan absensi: " + error.message);
  }
};

// Ambil daftar absensi berdasarkan ID karyawan
exports.getAbsensiByUser = async (id_karyawan) => {
  try {
    const snapshot = await db
      .collection("attendance")
      .where("id_karyawan", "==", id_karyawan)
      .orderBy("date", "desc")
      .get();

    return snapshot.docs.map((doc) => ({ id: doc.id, ...doc.data() }));
  } catch (error) {
    throw new Error("Gagal mengambil data absensi: " + error.message);
  }
};
