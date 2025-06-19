const { admin, db } = require("../config/firebase");

const authenticate = async (req, res, next) => {
	const token = req.headers.authorization?.split("Bearer ")[1];

	if (!token) {
		return res.status(401).json({ error: "Unauthorized. Token required." });
	}

	try {
		const decodedToken = await admin.auth().verifyIdToken(token);
		const userDoc = await db.collection("users").doc(decodedToken.uid).get();

		if (!userDoc.exists) {
			return res.status(404).json({ error: "User not found." });
		}

		const userData = userDoc.data();
		req.user = {
			...decodedToken,
			nama: userData.nama,
			fingerprint_id: userData.fingerprint_id,
		};

		next();
	} catch (error) {
		res.status(401).json({ error: "Unauthorized. Invalid token." });
	}
};
module.exports = authenticate;
