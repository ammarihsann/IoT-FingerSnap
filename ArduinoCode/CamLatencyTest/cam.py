import paho.mqtt.client as mqtt
from datetime import datetime
import time

# Variabel global untuk menyimpan ID terakhir
last_fingerprint_id = None
last_image_send_time_ms = None

# MQTT Callback saat terhubung
def on_connect(client, userdata, flags, rc):
    print("✅ Connected to MQTT broker with code:", rc)
    client.subscribe("/absensi")
    client.subscribe("/image/raw")

# MQTT Callback saat menerima pesan
def on_message(client, userdata, msg):
    global last_fingerprint_id, last_image_send_time_ms

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
        if len(msg.payload) < 8: # Assuming 8 bytes for timestamp
            print("❌ Payload gambar terlalu pendek untuk menyertakan timestamp.")
            return

        received_esp_cam_time_ms = int.from_bytes(msg.payload[:8], 'little')
        
        image_data = msg.payload[8:]

        current_receive_time_ms = int(time.time() * 1000) # Waktu terima di Python dalam ms

        latency_ms = current_receive_time_ms - received_esp_cam_time_ms
        print(f"⏱ Latensi pengiriman gambar (ESP-CAM ke MQTT Broker): {latency_ms} ms")

        if last_fingerprint_id is None:
            print("⚠ Gambar diterima tapi belum ada ID fingerprint!")
            filename = f"fp_unknown_{datetime.now().strftime('%Y%m%d_%H%M%S')}.jpg"
        else:
            filename = f"fp_{last_fingerprint_id}_{datetime.now().strftime('%Y%m%d_%H%M%S')}.jpg"
        
        with open(filename, "wb") as f:
            f.write(image_data)
        print(f"📸 Gambar disimpan sebagai {filename}")

        last_fingerprint_id = None
        last_image_send_time_ms = None

# Setup client
client = mqtt.Client()
client.username_pw_set("hivemq.webclient.XXXXXX", "XXXXXXXXXXXXXX") # Ganti dengan kredensial Anda
client.tls_set()
client.on_connect = on_connect
client.on_message = on_message

client.connect("XXXXXXXXX.s1.eu.hivemq.cloud", 8883, 60) # Ganti broker jika berbeda
client.loop_forever()
