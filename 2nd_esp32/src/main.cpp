#include <WiFi.h>
#include <WiFiMulti.h>
#include <PubSubClient.h>

#define RELAY_PIN 26 

WiFiMulti wifiMulti;
WiFiClient espClient;
PubSubClient mqttClient(espClient);

const char* mqtt_server = "192.168.1.7";
const int mqtt_port = 1883;
const char* mqtt_topic = "RFID_LOGIN";

struct WiFiCredentials {
    const char* ssid;
    const char* password;
};

WiFiCredentials networks[] = {
    {"Cloud Control Network", "ccv7network"},
    {"PLDTHOMEFIBR12920", "PLDTWIFI4t7hc"},
    {"Kyle's iPhone", "ksambz011"},
    {"Tip", "Rovic0311"},
    {"STUDENT-CONNECT", "IloveUSTP!"},
};

void reconnectMQTT();
void checkWiFiConnection();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void pulseRelayOn();   
void pulseRelayOff();  

void setup() {
    Serial.begin(115200);
    delay(100);
    
    Serial.println("\n\n=================================");
    Serial.println("ESP32 MQTT Relay Controller");
    Serial.println("IT414 FINAL PIT");
    Serial.println("CW-020 Low Level Trigger Mode");
    Serial.println("With Contact Pulse Workaround");
    Serial.println("=================================\n");
    
    Serial.println("Initializing relay pin...");
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, HIGH);
    delay(100);
    
    Serial.printf("✓ Relay Pin: GPIO %d initialized\n", RELAY_PIN);
    Serial.println("✓ Relay state: OFF (HIGH)\n");

    for (int i = 0; i < sizeof(networks)/sizeof(networks[0]); i++) {
        wifiMulti.addAP(networks[i].ssid, networks[i].password);
        Serial.printf("Added network: %s\n", networks[i].ssid);
    }

    Serial.println("\nConnecting to WiFi...");
    while (wifiMulti.run() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }

    Serial.println();
    Serial.printf("✓ WiFi connected to: %s\n", WiFi.SSID().c_str());
    Serial.printf("  IP address: %s\n", WiFi.localIP().toString().c_str());

    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttClient.setCallback(mqttCallback);
    
    Serial.println("\nConnecting to MQTT Broker...");
    reconnectMQTT();

    Serial.println("\nSystem Ready!");
    Serial.println("=================================\n");
}

void loop() {
    checkWiFiConnection();
    
    if (!mqttClient.connected()) {
        reconnectMQTT();
    }
    mqttClient.loop();
    
    delay(10);
}

void pulseRelayOn() {
    Serial.println("  [Pulsing relay contacts to ensure engagement...]");
    
    for (int i = 0; i < 3; i++) {
        digitalWrite(RELAY_PIN, HIGH);  
        delay(30);
        digitalWrite(RELAY_PIN, LOW);   
        delay(30);
    }
    
   
    digitalWrite(RELAY_PIN, LOW);
    delay(50);
    
 
    Serial.printf("  GPIO state: %s\n", digitalRead(RELAY_PIN) == LOW ? "LOW (should be ON)" : "HIGH");
}

void pulseRelayOff() {
   
    Serial.println("  [Pulsing relay contacts to disengage...]");
    
    for (int i = 0; i < 3; i++) {
        digitalWrite(RELAY_PIN, LOW);   
        delay(30);
        digitalWrite(RELAY_PIN, HIGH); 
        delay(30);
    }
    
    
    digitalWrite(RELAY_PIN, HIGH);
    delay(50);
    
   
    Serial.printf("  GPIO state: %s\n", digitalRead(RELAY_PIN) == HIGH ? "HIGH (should be OFF)" : "LOW");
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    Serial.println("=================================");
    Serial.printf("📨 Message: %s\n", topic);
    
    String message = "";
    for (int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    
    Serial.printf("Content: %s\n", message.c_str());
    
    if (message == "1") {
        Serial.println(">>> Command: RELAY ON");
        pulseRelayOn();  
        Serial.println("💡 LED: ON");
    } 
    else if (message == "0") {
        Serial.println(">>> Command: RELAY OFF");
        pulseRelayOff();  
        Serial.println("💡 LED: OFF");
    } 
    else {
        Serial.printf("⚠️  Unknown: %s\n", message.c_str());
    }
    
    Serial.println("=================================\n");
}

void reconnectMQTT() {
    int attempts = 0;
    while (!mqttClient.connected() && attempts < 5) {
        Serial.printf("MQTT attempt %d/5...\n", attempts + 1);
        
        String clientId = "ESP32_Relay_" + String(random(0xffff), HEX);
        
        if (mqttClient.connect(clientId.c_str())) {
            Serial.println("✓ MQTT Connected!");
            Serial.printf("  Broker: %s:%d\n", mqtt_server, mqtt_port);
            
            if (mqttClient.subscribe(mqtt_topic)) {
                Serial.printf("✓ Subscribed: %s\n", mqtt_topic);
            }
            return;
        } else {
            Serial.printf("✗ Failed, rc=%d\n", mqttClient.state());
            delay(2000);
        }
        attempts++;
    }
}

void checkWiFiConnection() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi lost. Reconnecting...");
        unsigned long startTime = millis();

        while (wifiMulti.run() != WL_CONNECTED && (millis() - startTime < 10000)) {
            delay(500);
            Serial.print(".");
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println();
            Serial.printf("✓ Reconnected: %s\n", WiFi.SSID().c_str());
            reconnectMQTT();
        }
    }
}