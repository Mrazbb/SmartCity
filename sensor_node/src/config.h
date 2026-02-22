#pragma once

// ==========================================
// CONFIGURATION
// ==========================================

// WiFi Settings
#define WIFI_SSID "Slow WIFI IoT"
#define WIFI_PASS "4hIJmOg70DS2nK"

// MQTT Settings
#define MQTT_SERVER "smartcity.marek-mraz.com"
#define MQTT_PORT 1883

// FIWARE IoT Agent Settings
#define FIWARE_API_KEY "4jggokgpepnvsb2uv4s40d59ov" // From iot-agent-json values.yaml
#define DEVICE_ID "AirQuality001"

// Hardware Pins (M5Stack Air Quality SKU:K131)
#define PIN_POWER_HOLD 46
#define PIN_SEN55_PWR 10
#define I2C_SDA 11
#define I2C_SCL 12

#define DEVICE_NAME "M5Stack Air Quality"
#define LATITUDE 38.0770502
#define LONGITUDE -1.2718119

// Sleep Settings
// How long the device should sleep between measurements (in minutes)
#define SLEEP_MINUTES 1

// Retry thresholds
#define MAX_WIFI_RETRIES 20
#define MAX_MQTT_RETRIES 5