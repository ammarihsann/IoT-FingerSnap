import paho.mqtt.client as mqtt
from datetime import datetime
import json
import os
import threading

# Variabel global
last_fingerprint_id = None
last_timestamp_str = None
SAVE_FOLDER = "images"
if not os.path.exists(SAVE_FOLDER):
    os.makedirs(SAVE_FOLDER)

# Statistik
absensi_count = 0
image_count = 0

# Timer untuk deteksi packet loss
IMAGE_TIMEOUT = 10  # detik
image_timer = None

def reset_image_timer():
    global image_timer
    if image_timer:
        image_timer.cancel()
    image_timer = threading.Timer(IMAGE_TIMEOUT, handle_missing_image)
    image_timer.start()

def handle_missing_image():
    global last_fingerprint_id, last_timestamp_str
    print(f"❌ Packet loss terdeteksi! Gambar untuk ID {last_fingerprint_id} tidak diterima dalam {IMAGE_TIMEOUT} detik.")
    print(f"📊 Statistik: Absensi={absensi_count}, Gambar={image_count}, Loss={absensi_count - image_count}")
    last_fingerprint_id = None
    last_timestamp_str = None

# MQTT Callback saat terhubung
def on_connect(client, userdata, flags, rc):
    print("✅ Connected to MQTT broker with code:", rc)
    client.subscribe("/absensi")
    client.subscribe("/image/raw")

# MQTT Callback saat menerima pesan
def on_message(client, userdata, msg):
    global last_fingerprint_id, last_timestamp_str
    global absensi_count, image_count, image_timer

    topic = msg.topic
    if topic == "/absensi":
        try:
            payload = msg.payload.decode("utf-8")
            data = json.loads(payload)
            last_fingerprint_id = data.get("fingerprint_id")
            last_timestamp_str = data.get("timestamp")  # Format: "2025-06-14 05:46:03"
            absensi_count += 1

            print(f"📥 Absensi diterima: ID={last_fingerprint_id}")

            # Hitung latency
            try:
                start_time = datetime.strptime(last_timestamp_str, "%Y-%m-%d %H:%M:%S")
                end_time = datetime.now()
                latency = (end_time - start_time).total_seconds()
                print(f"⏱️ Latency pengiriman absensi: {latency:.2f} detik")
            except Exception as e:
                print(f"❌ Gagal menghitung latency absensi: {e}")

            # Mulai timer untuk gambar
            reset_image_timer()

        except Exception as e:
            print("❌ Error parsing /absensi:", e)

    elif topic == "/image/raw":
        if last_fingerprint_id is None or last_timestamp_str is None:
            print("⚠ Gambar diterima tapi belum ada ID fingerprint atau timestamp!")
            return

        # Hentikan timer karena gambar diterima
        if image_timer:
            image_timer.cancel()

        # Simpan gambar
        try:
            filename = f"fp_{last_fingerprint_id}_{datetime.strptime(last_timestamp_str, '%Y-%m-%d %H:%M:%S').strftime('%Y%m%d_%H%M%S')}.jpg"
            filepath = os.path.join(SAVE_FOLDER, filename)
            with open(filepath, "wb") as f:
                f.write(msg.payload)

            print(f"📸 Gambar disimpan sebagai {filename}")

            # Hitung latency
            try:
                start_time = datetime.strptime(last_timestamp_str, "%Y-%m-%d %H:%M:%S")
                end_time = datetime.now()
                latency = (end_time - start_time).total_seconds()
                print(f"⏱️ Latency pengiriman gambar: {latency:.2f} detik")
            except Exception as e:
                print(f"❌ Gagal menghitung latency gambar: {e}")

            image_count += 1
            print(f"📊 Statistik: Absensi={absensi_count}, Gambar={image_count}, Loss={absensi_count - image_count}")

        except Exception as e:
            print(f"❌ Gagal menyimpan gambar: {e}")

        # Reset variabel fingerprint dan timestamp
        last_fingerprint_id = None
        last_timestamp_str = None

# Setup client
client = mqtt.Client()
client.username_pw_set("hivemq.webclient.XXXXXXXX", "MQTT_PASSWORD")
client.tls_set()
client.on_connect = on_connect
client.on_message = on_message

client.connect("XXXXXXXXXX.s1.eu.hivemq.cloud", 8883, 60)
client.loop_forever()
