const express = require("express");
const router = express.Router();
const scheduleController = require("../controllers/scheduleController");
const authMiddleware = require("../middlewares/authMiddleware");

// Middleware auth bisa dikondisikan untuk admin saja kalau diperlukan
router.post("/", authMiddleware, scheduleController.createSchedule);
router.get("/", authMiddleware, scheduleController.getAllSchedules);
router.put("/by-id/:id", authMiddleware, scheduleController.updateSchedule);
router.delete("/by-id/:id", authMiddleware, scheduleController.deleteSchedule);

module.exports = router;
