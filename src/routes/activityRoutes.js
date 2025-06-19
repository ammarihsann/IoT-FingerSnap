const express = require("express");
const router = express.Router();
const activityController = require("../controllers/activityController");

router.get("/recent", activityController.getRecentActivities);
router.get("/", activityController.getAllActivities);
router.delete("/reset", activityController.resetActivities);

module.exports = router;
