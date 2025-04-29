#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <esp_now.h>

// Konfigurasi WiFi
const char* ssid = "YOUR_SSID";  
const char* password = "YOUR_PASSWORD";  

// Konfigurasi MQTT
const char* mqtt_server = "mqtt_server.s1.eu.hivemq.cloud";  
const char* mqtt_username = "hivemq.webclient.username";  
const char* mqtt_password = "MQTT_PASSWORD";  
const char* mqtt_topic = "/enroll";  

WiFiClientSecure espClient;  
PubSubClient client(espClient);

// Konfigurasi ESP-NOW
uint8_t esp32BasementMac[] = { 0xCC, 0xDB, 0xA7, 0x3D, 0xD4, 0xDC };  

typedef struct {
    int fingerprint_id;
} AbsensiData;

AbsensiData absensiData;

void setup() {
    Serial.begin(115200);

    // Koneksi ke WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\n✅ WiFi Connected!");

    // Mengabaikan verifikasi sertifikat SSL/TLS untuk testing
    espClient.setInsecure();

    // Konfigurasi MQTT
    client.setServer(mqtt_server, 8883);
    client.setCallback(callback);
    connectToMQTT();

    // Inisialisasi ESP-NOW
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        Serial.println("❌ ESP-NOW Initialization failed!");
        return;
    }

    // Menambahkan ESP32 Basement sebagai peer
    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, esp32BasementMac, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("❌ Gagal menambahkan peer ESP32 Basement.");
    } else {
        Serial.println("✅ Peer ESP32 Basement berhasil ditambahkan.");
    }
}

// Fungsi untuk koneksi ke broker MQTT
void connectToMQTT() {
    while (!client.connected()) {
        Serial.print("Connecting to MQTT...");
        if (client.connect("ESP32u_Client", mqtt_username, mqtt_password)) {
            Serial.println("✅ Connected to MQTT!");
            client.subscribe(mqtt_topic);
        } else {
            Serial.print("❌ MQTT Failed, rc=");
            Serial.println(client.state());
            delay(5000);
        }
    }
}

// Callback untuk menerima pesan dari broker MQTT
void callback(char* topic, byte* payload, unsigned int length) {
    Serial.println("\n📥 Data MQTT diterima!");

    // Parsing JSON dengan cara yang benar
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, payload, length);

    if (error) {
        Serial.print("❌ JSON Parsing Error: ");
        Serial.println(error.f_str());
        return;
    }

    // Ambil fingerprint_id dari JSON
    if (doc.containsKey("fingerprint_id")) {
        absensiData.fingerprint_id = doc["fingerprint_id"];
        Serial.print("📌 Fingerprint ID yang diterima: ");
        Serial.println(absensiData.fingerprint_id);

        // Kirim data ke ESP32 Basement via ESP-NOW
        esp_err_t result = esp_now_send(esp32BasementMac, (uint8_t *)&absensiData, sizeof(AbsensiData));

        Serial.print("🔄 Mengirim data ke ESP32 Basement... ");
        if (result == ESP_OK) {
            Serial.println("✅ Sukses!");
        } else {
            Serial.println("❌ Gagal!");
        }
    } else {
        Serial.println("⚠️ Tidak ditemukan 'fingerprint_id' dalam JSON!");
    }
}

void loop() {
    if (!client.connected()) {
        connectToMQTT();
    }
    client.loop();
}
