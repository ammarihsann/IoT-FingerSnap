const scheduleService = require("../services/scheduleService");

exports.createSchedule = async (req, res) => {
	try {
		const { hari, jam_masuk, jam_keluar } = req.body;
		if (!hari || !jam_masuk || !jam_keluar) {
			return res
				.status(400)
				.json({ success: false, message: "Data tidak lengkap" });
		}

		const result = await scheduleService.createSchedule({
			hari,
			jam_masuk,
			jam_keluar,
		});
		res.status(201).json({
			success: true,
			message: "Jadwal berhasil ditambahkan",
			data: result,
		});
	} catch (err) {
		res.status(500).json({ success: false, message: err.message });
	}
};

exports.getAllSchedules = async (req, res) => {
	try {
		const schedules = await scheduleService.getAllSchedules();
		res.status(200).json({ success: true, data: schedules });
	} catch (err) {
		res.status(500).json({ success: false, message: err.message });
	}
};

exports.updateSchedule = async (req, res) => {
	try {
		const { id } = req.params;
		const { hari, jam_masuk, jam_keluar } = req.body;

		// Filter field undefined agar tidak dikirim ke Firestore
		const dataToUpdate = {};
		if (hari !== undefined) dataToUpdate.hari = hari;
		if (jam_masuk !== undefined) dataToUpdate.jam_masuk = jam_masuk;
		if (jam_keluar !== undefined) dataToUpdate.jam_keluar = jam_keluar;

		const result = await scheduleService.updateSchedule(id, dataToUpdate);

		res.status(200).json({
			success: true,
			message: "Jadwal berhasil diupdate",
			data: result,
		});
	} catch (err) {
		res.status(500).json({ success: false, message: err.message });
	}
};

exports.deleteSchedule = async (req, res) => {
	try {
		const { id } = req.params;
		await scheduleService.deleteSchedule(id);
		res.status(200).json({ success: true, message: "Jadwal berhasil dihapus" });
	} catch (err) {
		res.status(500).json({ success: false, message: err.message });
	}
};
