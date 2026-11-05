#ifndef SMARTHOME_CONFIG_H
#define SMARTHOME_CONFIG_H

#include <Arduino.h>

// Firmware version
#define FIRMWARE_VERSION "2.0.0-PRO"
#define DEVICE_NAME      "SmartHome ESP32 Hub"

// Default WiFi AP (Combination Mode fallback)
#define DEFAULT_AP_SSID_PREFIX "SmartHome-Hub-"
#define DEFAULT_AP_PASS        "12345678"
#define DEFAULT_AP_CHANNEL     1
#define DEFAULT_AP_MAX_CONN    8

// Default WiFi STA (Home router)
#define DEFAULT_STA_SSID       ""
#define DEFAULT_STA_PASS       ""

// Default Admin account
#define DEFAULT_ADMIN_USER     "admin"
#define DEFAULT_ADMIN_PASS     "admin123"
#define DEFAULT_ADMIN_NAME     "Administrator"

// Default Demo Family accounts (can be modified/deleted by admin in UI)
#define DEFAULT_USER1_USER     "alice"
#define DEFAULT_USER1_PASS     "alice123"
#define DEFAULT_USER1_NAME     "Alice (Bed 1)"
#define DEFAULT_USER1_ROOMS    "bedroom_1,living_room"

#define DEFAULT_USER2_USER     "bob"
#define DEFAULT_USER2_PASS     "bob123"
#define DEFAULT_USER2_NAME     "Bob (Bed 2)"
#define DEFAULT_USER2_ROOMS    "bedroom_2,living_room"

// Hardware GPIO pin assignments (ESP32)
// Built-in LED on most ESP32 boards is GPIO 2
#define PIN_LIGHT_LIVING       2   // GPIO 2 - Built-in LED / Living Room Main
#define PIN_LIGHT_BED1         4   // GPIO 4 - Master Bedroom Light
#define PIN_LIGHT_BED2         16  // GPIO 16 - Bedroom 2 Light
#define PIN_LIGHT_KITCHEN      17  // GPIO 17 - Kitchen Light
#define PIN_LIGHT_BALCONY      18  // GPIO 18 - Balcony Light
#define PIN_DIMMER_LIVING      19  // GPIO 19 - Living Room Dimmer (PWM)
#define PIN_FAN_LIVING         21  // GPIO 21 - Living Room Fan / Relay

// PWM properties for dimmer
#define PWM_CHANNEL            0
#define PWM_FREQ               5000
#define PWM_RESOLUTION         8

// Web Server
#define HTTP_PORT              80
#define WS_PATH                "/ws"
#define MDNS_HOST              "smarthome"

// Token expiration (7 days)
#define SESSION_EXPIRY_MS      (7ULL * 24 * 3600 * 1000)

#endif // SMARTHOME_CONFIG_H
