const { db } = require("../config/firebase");

// Helper untuk generate ID otomatis: SCH-01, SCH-02, ...
const generateScheduleId = async () => {
  const snapshot = await db
    .collection("schedules")
    .orderBy("id", "desc")
    .limit(1)
    .get();
  if (snapshot.empty) return "SCH-01";

  const lastId = snapshot.docs[0].data().id; // e.g. SCH-09
  const lastNumber = parseInt(lastId.split("-")[1]);
  const newId = `SCH-${String(lastNumber + 1).padStart(2, "0")}`;
  return newId;
};

exports.createSchedule = async ({ hari, jam_masuk, jam_keluar }) => {
  const newId = await generateScheduleId();

  const newSchedule = {
    id: newId, // ← Ini ID utama yang digunakan sebagai primary key
    hari,
    jam_masuk,
    jam_keluar,
    created_at: new Date(),
  };

  const docRef = await db.collection("schedules").add(newSchedule);
  return { docId: docRef.id, ...newSchedule };
};

exports.getAllSchedules = async () => {
  const snapshot = await db.collection("schedules").get();
  return snapshot.docs.map((doc) => ({ id: doc.id, ...doc.data() }));
};

exports.updateSchedule = async (id, data) => {
  const snapshot = await db
    .collection("schedules")
    .where("id", "==", id)
    .limit(1)
    .get();
  if (snapshot.empty) throw new Error("Jadwal tidak ditemukan");

  const docRef = snapshot.docs[0].ref;
  await docRef.update(data);
  const updatedDoc = await docRef.get();
  return { id: updatedDoc.data().id, ...updatedDoc.data() };
};
exports.deleteSchedule = async (id) => {
  const snapshot = await db
    .collection("schedules")
    .where("id", "==", id)
    .limit(1)
    .get();
  if (snapshot.empty) throw new Error("Jadwal tidak ditemukan");

  const docRef = snapshot.docs[0].ref;
  await docRef.delete();
};
