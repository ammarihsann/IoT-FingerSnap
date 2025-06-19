const express = require("express");
const cors = require("cors");
const path = require("path");
const authRoutes = require("./routes/authRoutes");
const userRoutes = require("./routes/userRoutes");
const absensiRoutes = require("./routes/absensiRoutes");
const scheduleRoutes = require("./routes/scheduleRoutes");
const mqttRoutes = require("./routes/mqttRoutes");
const activityRoutes = require("./routes/activityRoutes");
const settingsRoutes = require("./routes/settingsRoutes");

const app = express();
app.use(
	cors({
		origin: [
			"https://ptbit-backend-874259397350.asia-southeast2.run.app",
			"http://localhost:8080",
		],
	})
);
app.use(express.json());

// Static files untuk dashboard admin
app.use(
	"/admin",
	express.static(path.join(__dirname, "../public/admin-dashboard"))
);

app.use(express.static(path.join(__dirname, "../public")));

app.get("/", (req, res) => {
	res.redirect("/admin/login.html");
});

// Routes API
app.use("/api/auth", authRoutes);
app.use("/api/absensi", absensiRoutes);
app.use("/api/mqtt", mqttRoutes); // nanti MQTT router bisa dipakai untuk testing juga
app.use("/api/users", userRoutes);
app.use("/api/schedules", scheduleRoutes);
app.use("/api/activities", activityRoutes);
app.use("/api/settings", settingsRoutes);

module.exports = app;
