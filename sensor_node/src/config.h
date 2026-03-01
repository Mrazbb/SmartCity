#pragma once

// ==========================================
// CONFIGURATION
// ==========================================

#include "secrets.h"

// WiFi Settings
// Moved to secrets.h

// MQTT Settings
#define MQTT_SERVER "smartcity.marek-mraz.com"
#define MQTT_PORT 1883

// FIWARE IoT Agent Settings
// FIWARE_API_KEY moved to secrets.h
#define DEVICE_ID "M5Stack003"

// Hardware Pins (M5Stack Air Quality SKU:K131)
#define PIN_POWER_HOLD 46
#define PIN_SEN55_PWR 10
#define PIN_USER_BUTTON_POWER 42

// External I2C (Grove)
#define I2C_EXT_SDA 13
#define I2C_EXT_SCL 15

// Internal I2C for SCD40, SEN55 & RTC
#define I2C_INT_SDA 11
#define I2C_INT_SCL 12

#define DEVICE_NAME "M5Stack Air Quality"
#define LATITUDE 48.7383681
#define LONGITUDE 19.1563278

// Sleep Settings
// How long the device should sleep between measurements (in minutes)
#define SLEEP_MINUTES 1

// In milliseconds
#define SLEEP_MS (SLEEP_MINUTES * 60 * 1000)

// Retry thresholds
#define MAX_WIFI_RETRIES 20
#define MAX_MQTT_RETRIES 5