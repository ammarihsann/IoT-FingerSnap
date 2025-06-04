import paho.mqtt.client as mqtt
from datetime import datetime

# Variabel global untuk menyimpan ID terakhir
last_fingerprint_id = None

# MQTT Callback saat terhubung
def on_connect(client, userdata, flags, rc):
    print("✅ Connected to MQTT broker with code:", rc)
    client.subscribe("/absensi")
    client.subscribe("/image/raw")

# MQTT Callback saat menerima pesan
def on_message(client, userdata, msg):
    global last_fingerprint_id

    topic = msg.topic
    if topic == "/absensi":
        try:
            payload = msg.payload.decode("utf-8")
            import json
            data = json.loads(payload)
            last_fingerprint_id = data.get("fingerprint_id")
            print(f"📥 Absensi diterima: ID={last_fingerprint_id}")
        except Exception as e:
            print("❌ Error parsing /absensi:", e)

    elif topic == "/image/raw":
        if last_fingerprint_id is None:
            print("⚠ Gambar diterima tapi belum ada ID fingerprint!")
            return

        filename = f"fp_{last_fingerprint_id}_{datetime.now().strftime('%Y%m%d_%H%M%S')}.jpg"
        with open(filename, "wb") as f:
            f.write(msg.payload)
        print(f"📸 Gambar disimpan sebagai {filename}")

        # Reset agar tidak tertukar jika ada delay antar data
        last_fingerprint_id = None

# Setup client
client = mqtt.Client()
client.username_pw_set("hivemq.webclient.XXXXXX", "XXXXXXXXXXXXXX")
client.tls_set()  # karena HiveMQ pakai TLS
client.on_connect = on_connect
client.on_message = on_message

client.connect("20b4045b4af5432c9978e9c8d1f97a15.s1.eu.hivemq.cloud", 8883, 60)
client.loop_forever()
