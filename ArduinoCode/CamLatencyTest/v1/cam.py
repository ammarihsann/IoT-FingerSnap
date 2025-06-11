import paho.mqtt.client as mqtt
from datetime import datetime
import json

# Variabel global
last_fingerprint_id = None
last_rtc_timestamp_str = None  # format: "YYYY-MM-DD HH:MM:SS"

# MQTT callback saat koneksi berhasil
def on_connect(client, userdata, flags, rc):
    print("✅ Connected to MQTT broker with code:", rc)
    client.subscribe("/absensi")
    client.subscribe("/image/meta")
    client.subscribe("/image/raw")

# MQTT callback saat pesan diterima
def on_message(client, userdata, msg):
    global last_fingerprint_id, last_rtc_timestamp_str

    topic = msg.topic
    payload = msg.payload

    if topic == "/absensi":
        try:
            data = json.loads(payload.decode())
            last_fingerprint_id = data.get("fingerprint_id")
            print(f"📥 Absensi diterima: ID={last_fingerprint_id}")
        except Exception as e:
            print("❌ Error parsing /absensi:", e)

    elif topic == "/image/meta":
        try:
            data = json.loads(payload.decode())
            last_fingerprint_id = data.get("id")
            last_rtc_timestamp_str = data.get("timestamp")

            print(f"🕒 Metadata gambar: ID={last_fingerprint_id}, timestamp={last_rtc_timestamp_str}")
        except Exception as e:
            print("❌ Error parsing /image/meta:", e)

    elif topic == "/image/raw":
        if not last_fingerprint_id or not last_rtc_timestamp_str:
            print("⚠ Gambar diterima tapi metadata tidak lengkap")
            return

        # Hitung latency
        try:
            timestamp_sent = datetime.strptime(last_rtc_timestamp_str, "%Y-%m-%d %H:%M:%S")
            timestamp_received = datetime.now()
            latency = (timestamp_received - timestamp_sent).total_seconds()

            filename = f"fp_{last_fingerprint_id}_{timestamp_received.strftime('%Y%m%d_%H%M%S')}.jpg"
            with open(filename, "wb") as f:
                f.write(payload)

            print(f"📸 Gambar disimpan sebagai {filename}")
            print(f"⏱️ Latency: {latency:.3f} detik\n")

        except Exception as e:
            print("❌ Error menghitung latency:", e)

        # Reset
        last_fingerprint_id = None
        last_rtc_timestamp_str = None

# Setup MQTT
client = mqtt.Client()
client.username_pw_set("hivemq.webclient.XXXXXXXXXXXXXXXXX", "PASSWORD")
client.tls_set()  # HiveMQ menggunakan TLS
client.on_connect = on_connect
client.on_message = on_message

client.connect("XXXXXXXXXXXXXXXXXXXX.s1.eu.hivemq.cloud", 8883, 60)
client.loop_forever()
