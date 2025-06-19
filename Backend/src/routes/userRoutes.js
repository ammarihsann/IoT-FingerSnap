const express = require("express");
const router = express.Router();
const userController = require("../controllers/userController");
const authMiddleware = require("../middlewares/authMiddleware");

// UPDATE username mobile
router.put("/update-username", authMiddleware, userController.updateUsername);

// UPDATE password mobile
router.put("/update-password", authMiddleware, userController.updatePassword);

// GET semua pegawai
router.get("/", authMiddleware, userController.getAllUsers);

// EDIT user
router.put("/:id", authMiddleware, userController.updateUser);

// DELETE user
router.delete("/:id", authMiddleware, userController.deleteUser);

module.exports = router;
