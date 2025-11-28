#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <MFRC522.h>
#include <PubSubClient.h>

#define SS_PIN 21
#define RST_PIN 22

WiFiMulti wifiMulti;
HTTPClient http;
MFRC522 mfrc522(SS_PIN, RST_PIN);
WiFiClient espClient;
PubSubClient mqttClient(espClient);

const char* serverHost = "192.168.1.7";
const char* serverPath = "/it414_final_pit/rfid_handler.php";

const char* mqtt_server = "192.168.1.7";
const int mqtt_port = 1883;
const char* mqtt_topic = "RFID_LOGIN";

struct WiFiCredentials {
    const char* ssid;
    const char* password;
};

WiFiCredentials networks[] = {
    {"Cloud Control Network", "ccv7network"},
    {"Tip", "Rovic0311"},
    {"Kyle’s iPhone", "ksambz011"},
    {"STUDENT-CONNECT", "IloveUSTP!"},
    {"PLDTHOMEFIBR12920", "PLDTWIFI4t7hc"},
};

void reconnectMQTT();
void sendRFIDData(String rfidData);
void checkWiFiConnection();
void publishMQTT(String message);

void setup() {
    Serial.begin(115200);
    Serial.println("\n\n=================================");
    Serial.println("ESP32 RFID MQTT System Starting...");
    Serial.println("IT414 FINAL PIT");
    Serial.println("=================================\n");
    
    SPI.begin();
    mfrc522.PCD_Init();
    Serial.println("RFID Scanner initialized");

    for (int i = 0; i < sizeof(networks)/sizeof(networks[0]); i++) {
        wifiMulti.addAP(networks[i].ssid, networks[i].password);
        Serial.printf("Added network: %s\n", networks[i].ssid);
    }

    Serial.println("Connecting to WiFi...");
    while (wifiMulti.run() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }

    Serial.println();
    Serial.printf("✓ WiFi connected to: %s\n", WiFi.SSID().c_str());
    Serial.printf("  IP address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("  Signal strength: %d dBm\n", WiFi.RSSI());

    mqttClient.setServer(mqtt_server, mqtt_port);
    
    Serial.println("\nConnecting to MQTT Broker...");
    reconnectMQTT();

    Serial.println("\nSystem Ready!");
    Serial.println("Waiting for RFID cards...");
    Serial.println("=================================\n");
}

void loop() {
    checkWiFiConnection();
    
    if (!mqttClient.connected()) {
        reconnectMQTT();
    }
    mqttClient.loop();

    if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
        delay(100);
        return;
    }

    String rfidData = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
        if (mfrc522.uid.uidByte[i] < 0x10) {
            rfidData += "0";
        }
        rfidData += String(mfrc522.uid.uidByte[i], HEX);
    }
    rfidData.toUpperCase();

    Serial.println("=================================");
    Serial.printf("🔍 RFID Card Detected: %s\n", rfidData.c_str());

    sendRFIDData(rfidData);

    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();

    delay(2000);
}

void sendRFIDData(String rfidData) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("ERROR: No WiFi connection");
        return;
    }

    String serverURL = String("http://") + serverHost + serverPath;
    http.begin(serverURL);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("User-Agent", "ESP32-RFID-Client");
    http.setTimeout(10000);

    StaticJsonDocument<200> doc;
    doc["rfid_data"] = rfidData;
    String requestBody;
    serializeJson(doc, requestBody);

    Serial.printf("📤 Sending to server: %s\n", serverURL.c_str());

    int httpResponseCode = http.POST(requestBody);

    if (httpResponseCode > 0) {
        String response = http.getString();
        
        StaticJsonDocument<512> responseDoc;
        DeserializationError error = deserializeJson(responseDoc, response);

        if (!error) {
            bool found = responseDoc["success"] | false;   
            int status = responseDoc["status"] | 0;        
            const char* message = responseDoc["message"] | "";

            if (found) {
                Serial.println("✓ RFID FOUND IN DATABASE");
                Serial.printf("  New Status: %d\n", status);
                publishMQTT(String(status));
            } else {
                Serial.println("✗ RFID NOT FOUND");
                publishMQTT(String(status));
            }

            Serial.printf("  Message: %s\n", message);

            if (responseDoc["time_log"].is<const char*>()) {
                Serial.printf("  Logged at: %s\n", responseDoc["time_log"].as<const char*>());
            }

        } else {
            Serial.printf("ERROR: JSON parsing failed (%s)\n", error.c_str());
            publishMQTT("0"); 
        }
    } else {
        Serial.printf("ERROR: HTTP request failed, code: %d\n", httpResponseCode);
        publishMQTT("0");
    }

    http.end();
    Serial.println("=================================\n");
}

void publishMQTT(String message) {
    if (mqttClient.connected()) {
        if (mqttClient.publish(mqtt_topic, message.c_str())) {
            Serial.printf("📡 MQTT Published: %s to topic %s\n", message.c_str(), mqtt_topic);
        } else {
            Serial.println("ERROR: MQTT publish failed");
        }
    } else {
        Serial.println("ERROR: MQTT not connected");
        reconnectMQTT();
    }
}

void reconnectMQTT() {
    int attempts = 0;
    while (!mqttClient.connected() && attempts < 5) {
        Serial.printf("Attempting MQTT connection (attempt %d/5)...\n", attempts + 1);
        
        String clientId = "ESP32_RFID_" + String(random(0xffff), HEX);
        
        if (mqttClient.connect(clientId.c_str())) {
            Serial.println("✓ MQTT Connected!");
            Serial.printf("  Client ID: %s\n", clientId.c_str());
            Serial.printf("  Broker: %s:%d\n", mqtt_server, mqtt_port);
            Serial.printf("  Topic: %s\n", mqtt_topic);
            return;
        } else {
            Serial.printf("✗ MQTT Connection failed, rc=%d\n", mqttClient.state());
            Serial.println("  Trying again in 2 seconds...");
            delay(2000);
        }
        attempts++;
    }
    
    if (!mqttClient.connected()) {
        Serial.println("WARNING: Could not connect to MQTT broker after 5 attempts");
        Serial.println("         System will continue without MQTT");
    }
}

void checkWiFiConnection() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi connection lost. Reconnecting...");
        unsigned long startTime = millis();

        while (wifiMulti.run() != WL_CONNECTED && (millis() - startTime < 10000)) {
            delay(500);
            Serial.print(".");
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println();
            Serial.printf("✓ Reconnected to: %s\n", WiFi.SSID().c_str());
            reconnectMQTT();
        } else {
            Serial.println();
            Serial.println("✗ Failed to reconnect to WiFi");
        }
    }
}
