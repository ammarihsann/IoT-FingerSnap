import requests
from flask import Flask, request, jsonify, send_from_directory
from datetime import datetime
import mysql.connector

app = Flask(__name__)

# Ganti dengan IP ESP32-U
ESP32U_URL = 'http://192.168.100.206/enroll'

# Konfigurasi database (ganti sesuai kebutuhan)
DB_CONFIG = {
    'host': 'localhost',
    'user': 'root',
    'password': '',
    'database': 'sistem_absensi'
}

@app.route("/")
def index():
    return send_from_directory("static", "index.html")

# Serve file statis dari folder "static"
@app.route("/<path:filename>")
def serve_static(filename):
    return send_from_directory("static", filename)

# Endpoint untuk mengirim data ke ESP32-U
@app.route('/enroll', methods=['POST'])
def enroll():
    data = request.get_json()
    fingerprint_id = data.get("fingerprint_id")

    if not fingerprint_id:
        return jsonify({'message': 'fingerprint_id missing'}), 400

    try:
        r = requests.post(ESP32U_URL, json={"fingerprint_id": fingerprint_id}, timeout=5)
        return jsonify({'message': 'Dikirim ke ESP32-U', 'response': r.json()})
    except requests.exceptions.RequestException as e:
        return jsonify({'error': str(e)}), 500

# Endpoint untuk menyimpan data user baru ke database
@app.route("/register", methods=["POST"])
def register():
    data = request.get_json()

    if "fingerprint_id" not in data or "nama" not in data:
        return jsonify({"message": "❌ Data tidak valid!"}), 400

    nama = data["nama"].strip()
    try:
        fingerprint_id = int(data["fingerprint_id"])
    except ValueError:
        return jsonify({"message": "❌ Fingerprint ID harus berupa angka!"}), 400

    if not nama or not (1 <= fingerprint_id <= 127):
        return jsonify({"message": "❌ Fingerprint ID harus antara 1-127 dan nama tidak boleh kosong!"}), 400

    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    try:
        conn = mysql.connector.connect(**DB_CONFIG)
        cursor = conn.cursor()

        sql = "INSERT INTO employees (fingerprint_id, nama, timestamp) VALUES (%s, %s, %s)"
        cursor.execute(sql, (fingerprint_id, nama, timestamp))
        conn.commit()

        cursor.close()
        conn.close()

        return jsonify({"message": "✅ Data berhasil disimpan ke database!"})

    except Exception as e:
        return jsonify({"message": f"❌ Error: {str(e)}"}), 500

# Jalankan Flask
if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
