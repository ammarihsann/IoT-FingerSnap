const express = require("express");
const router = express.Router();
const { db } = require("../config/firebase");

// GET batas jam checkin/checkout
router.get("/absensi-batas-jam", async (req, res) => {
	const doc = await db.collection("settings").doc("absensi").get();
	const data = doc.exists ? doc.data() : {};
	res.json({ success: true, batas_jam: data.batas_jam || "12:00" });
});

// UPDATE batas jam checkin/checkout
router.post("/absensi-batas-jam", async (req, res) => {
	const { batas_jam } = req.body;
	if (!batas_jam)
		return res
			.status(400)
			.json({ success: false, message: "Batas jam harus diisi" });
	await db
		.collection("settings")
		.doc("absensi")
		.set({ batas_jam }, { merge: true });
	res.json({ success: true, batas_jam });
});

module.exports = router;
