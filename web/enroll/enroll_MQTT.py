from flask import Flask, request, jsonify, send_from_directory
import paho.mqtt.client as mqtt
import mysql.connector
import ssl
from datetime import datetime
import json

app = Flask(__name__)

# Konfigurasi Database MySQL
DB_CONFIG = {
    "host": "localhost",
    "user": "root",
    "password": "",  # Ganti dengan password MySQL Anda
    "database": "sistem_absensi"
}

# Konfigurasi MQTT
MQTT_BROKER = "mqtt_broker.s1.eu.hivemq.cloud"
MQTT_USERNAME = "hivemq.webclient.username"
MQTT_PASSWORD = "password"
TOPIC = "/enroll"

# Inisialisasi MQTT Client
client = mqtt.Client(client_id="", protocol=mqtt.MQTTv311)
client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)
client.tls_set_context(ssl.create_default_context())  # Gunakan SSL/TLS
client.connect(MQTT_BROKER, 8883, 60)
client.loop_start()  # Memastikan koneksi tetap berjalan

# Route utama (menampilkan halaman HTML)
@app.route("/")
def index():
    return send_from_directory("static", "index.html")

# Route untuk menyajikan file CSS & JS
@app.route("/<path:filename>")
def serve_static(filename):
    return send_from_directory("static", filename)

# Endpoint untuk mengirim Fingerprint ID ke MQTT
@app.route("/enroll", methods=["POST"])
def enroll():
    data = request.json
    if "fingerprint_id" not in data:
        return jsonify({"message": "Fingerprint ID diperlukan!"}), 400

    fingerprint_id = data["fingerprint_id"]

    try:
        # Kirim data ke MQTT dalam format JSON
        payload = json.dumps({"fingerprint_id": fingerprint_id})
        result = client.publish(TOPIC, payload)
        result.wait_for_publish()  # Pastikan pesan benar-benar terkirim

        return jsonify({"message": f"✅ Fingerprint ID {fingerprint_id} berhasil dikirim ke MQTT!"})

    except Exception as e:
        return jsonify({"message": f"❌ Error: {str(e)}"}), 500

# Endpoint untuk menyimpan data ke database
@app.route("/register", methods=["POST"])
def register():
    data = request.json
    if "fingerprint_id" not in data or "nama" not in data:
        return jsonify({"message": "❌ Data tidak valid!"}), 400

    nama = data["nama"].strip()
    try:
        fingerprint_id = int(data["fingerprint_id"])  # <-- Perbaikan di sini
    except ValueError:
        return jsonify({"message": "❌ Fingerprint ID harus berupa angka!"}), 400

    if not nama or not (1 <= fingerprint_id <= 127):
        return jsonify({"message": "❌ Fingerprint ID harus antara 1-127 dan nama tidak boleh kosong!"}), 400

    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    try:
        # Koneksi ke database
        conn = mysql.connector.connect(**DB_CONFIG)
        cursor = conn.cursor()

        # Simpan ke database
        sql = "INSERT INTO employees (fingerprint_id, nama, timestamp) VALUES (%s, %s, %s)"
        cursor.execute(sql, (fingerprint_id, nama, timestamp))
        conn.commit()

        # Tutup koneksi
        cursor.close()
        conn.close()

        return jsonify({"message": "✅ Data berhasil disimpan ke database!"})

    except Exception as e:
        return jsonify({"message": f"❌ Error: {str(e)}"}), 500


if __name__ == "__main__":
    app.run(debug=True)
