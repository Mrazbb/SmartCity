#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <SensirionI2CSen5x.h>
#include <SensirionI2CScd4x.h>
#include "config.h"

WiFiClient espClient;
PubSubClient mqttClient(espClient);
SensirionI2CSen5x sen5x;
SensirionI2CScd4x scd4x;

// Helper to shut down device
void enterSleep() {
    Serial.printf("Entering sleep for %d minutes...\n", SLEEP_MINUTES);
    
    // Stop sensors
    scd4x.stopPeriodicMeasurement();
    sen5x.stopMeasurement();

    // Release power hold to shut down completely (if running on battery)
    // The device hardware will turn off if not connected to USB
    digitalWrite(PIN_SEN55_PWR, HIGH);
    digitalWrite(PIN_USER_BUTTON_POWER, LOW);
    digitalWrite(PIN_POWER_HOLD, LOW);

    // If connected to USB, power won't turn off, so we need to use deep sleep
    // M5.Power.timerSleep does not always work reliably for K131 module
    Serial.println("Entering ESP32 Deep Sleep...");
    delay(500); // Allow serial to flush
    
    // ESP32 Deep Sleep fallback
    esp_sleep_enable_timer_wakeup(SLEEP_MINUTES * 60 * 1000000ULL);
    esp_deep_sleep_start();
}

bool connectWiFi() {
    Serial.print("Connecting to WiFi: ");
    Serial.println(WIFI_SSID);
    if (WIFI_PASS == NULL || strlen(WIFI_PASS) == 0) {
        WiFi.begin(WIFI_SSID);
    } else {
        WiFi.begin(WIFI_SSID, WIFI_PASS);
    }

    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < MAX_WIFI_RETRIES) {
        delay(500);
        Serial.print(".");
        retries++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected.");
        return true;
    }
    Serial.println("\nWiFi connection failed.");
    return false;
}

bool connectMQTT() {
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    int retries = 0;
    while (!mqttClient.connected() && retries < MAX_MQTT_RETRIES) {
        Serial.print("Connecting to MQTT...");
        if (mqttClient.connect(DEVICE_ID)) {
            Serial.println("connected");
            return true;
        } else {
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" retrying in 2 seconds");
            delay(2000);
            retries++;
        }
    }
    return false;
}

void setup() {
    // 1. Initialize M5Stack (keeps power alive via internal M5 logic if applicable)
    auto cfg = M5.config();
    // cfg.internal_display = false; // (Not available in this version of M5Unified)
    cfg.external_display_value = 0;
    cfg.clear_display = false;
    M5.begin(cfg);
    Serial.begin(115200);
    delay(1000);

    // Ensure power is held BEFORE we start doing anything else
    // This is required for the K131 module to stay on when on battery
    pinMode(PIN_POWER_HOLD, OUTPUT);
    digitalWrite(PIN_POWER_HOLD, HIGH);

    Serial.println("--- Air Quality Sensor Booting ---");

    // 2. Hardware Power Management (Crucial for SKU:K131)
    pinMode(PIN_USER_BUTTON_POWER, OUTPUT);
    digitalWrite(PIN_USER_BUTTON_POWER, HIGH); // Turn on User Button Power (often required for I2C pullups or 5V boost)

    pinMode(PIN_SEN55_PWR, OUTPUT);
    digitalWrite(PIN_SEN55_PWR, LOW);  // Turn on SEN55
    
    // Give SEN55 time to boot up after power is applied
    delay(1200);

    // 3. Initialize I2C and Sensors
    Wire.end(); // Force reset of I2C to ensure correct pins
    Wire.begin(I2C_INT_SDA, I2C_INT_SCL);   // Internal I2C for SCD40 & SEN55
    
    // Initialize external I2C for Grove port
    Wire1.begin(I2C_EXT_SDA, I2C_EXT_SCL);

    scd4x.begin(Wire);
    sen5x.begin(Wire);
    
    // Start Measurements
    Serial.println("Starting sensors...");
    scd4x.stopPeriodicMeasurement();
    sen5x.deviceReset();
    delay(100);

    scd4x.startPeriodicMeasurement();
    sen5x.startMeasurement();

    // The SCD40 needs ~5 seconds for the first reading
    Serial.println("Waiting 6 seconds for sensor stabilization...");
    delay(6000);

    // 4. Read Sensors
    uint16_t co2 = 0;
    float scd_temp = 0.0f, scd_hum = 0.0f;
    bool scd_valid = (scd4x.readMeasurement(co2, scd_temp, scd_hum) == 0);

    float pm1p0 = 0, pm2p5 = 0, pm4p0 = 0, pm10p0 = 0;
    float sen_hum = 0, sen_temp = 0, voc = 0, nox = 0;
    bool sen_valid = (sen5x.readMeasuredValues(pm1p0, pm2p5, pm4p0, pm10p0, sen_hum, sen_temp, voc, nox) == 0);


    StaticJsonDocument<512> doc;
    doc["battery"] = M5.Power.getBatteryLevel();
    doc["battery_voltage"] = M5.Power.getBatteryVoltage();

    // 5. Build JSON Payload for FIWARE IoT Agent
    if (scd_valid) {
        doc["co2"] = serialized(String((float)co2, 1));
        doc["temperature"] = serialized(String(scd_temp, 2));
        doc["humidity"] = serialized(String(scd_hum, 2));
    }
    if (sen_valid) {
        // Prefer SEN55 temp/humidity if valid, or just add PM/VOC
        doc["pm1"] = serialized(String(pm1p0, 1));
        doc["pm25"] = serialized(String(pm2p5, 1));
        doc["pm4"] = serialized(String(pm4p0, 1));
        doc["pm10"] = serialized(String(pm10p0, 1));
        if (!isnan(sen_temp)) doc["sen_temp"] = serialized(String(sen_temp, 2));
        if (!isnan(sen_hum)) doc["sen_hum"] = serialized(String(sen_hum, 2));
        if (!isnan(voc)) doc["voc"] = serialized(String(voc, 1));
        if (!isnan(nox)) doc["nox"] = serialized(String(nox, 1));
    }

    // 6. Connect & Publish (Only if we have valid data)
    if ((scd_valid || sen_valid) && connectWiFi()) {
        doc["rssi"] = WiFi.RSSI();
        
        String payload;
        serializeJson(doc, payload);
        Serial.println("Payload: " + payload);

        if (connectMQTT()) {
            // IoT Agent JSON standard topic format: /<API_KEY>/<DEVICE_ID>/attrs
            String topic = String("/") + FIWARE_API_KEY + "/" + DEVICE_ID + "/attrs";
            
            if (mqttClient.publish(topic.c_str(), payload.c_str())) {
                Serial.println("Successfully published to MQTT.");
            } else {
                Serial.println("MQTT Publish failed.");
            }
            
            // Allow time for message to dispatch
            mqttClient.loop();
            delay(500);
            mqttClient.disconnect();
        }
        WiFi.disconnect(true);
    } else {
        Serial.println("Failed to read sensors or connect to network. Skipping publish.");
    }

    // 7. Go to Sleep
    enterSleep();
}

void loop() {
    // Left empty: Device operates in a boot -> measure -> publish -> sleep cycle
}