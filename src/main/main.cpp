#include <Arduino.h>
#include <ESPmDNS.h>
#include "config.h"
#include "wifi_manager.h"
#include "auth_manager.h"
#include "device_manager.h"
#include "web_server.h"
#include "alexa_manager.h"
#include "button_manager.h"

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n==================================================");
  Serial.printf("   %s v%s\n", DEVICE_NAME, FIRMWARE_VERSION);
  Serial.println("   Powered by Mongoose.ws Embedded Networking");
  Serial.println("==================================================");

  // 1. Initialize WiFi in Combination Mode (AP + STA)
  WifiManager::getInstance().begin();

  // 2. Start mDNS Responder (http://smarthome.local)
  if (MDNS.begin(MDNS_HOST)) {
    MDNS.addService("http", "tcp", HTTP_PORT);
    Serial.printf("[mDNS] Responder started: http://%s.local\n", MDNS_HOST);
  } else {
    Serial.println("[mDNS] Error setting up mDNS responder!");
  }

  // 3. Initialize Multi-User RBAC & NVS persistence
  AuthManager::getInstance().begin();

  // 4. Initialize Hardware GPIOs, Rooms, Devices & Sensors
  DeviceManager::getInstance().begin();

  // 5. Start Mongoose HTTP & WebSocket Web Server
  WebServer::getInstance().begin();

  // 6. Initialize Alexa Echo Local Voice Assistant Bridge
  AlexaManager::getInstance().begin();

  // 7. Initialize Physical Input Buttons & Wall Switches
  ButtonManager::getInstance().begin();

  Serial.println("==================================================");
  Serial.printf(" [READY] Access Web UI / Android App at:\n");
  Serial.printf("   Direct AP:  http://%s\n", WifiManager::getInstance().getApIp().c_str());
  Serial.printf("   Local mDNS: http://%s.local\n", MDNS_HOST);
  Serial.println("==================================================\n");
}

void loop() {
  // Poll Mongoose network events
  WebServer::getInstance().poll();

  // Update dynamic sensor readings and check timers
  DeviceManager::getInstance().loop();

  // Monitor and handle WiFi auto-reconnect
  WifiManager::getInstance().loop();

  // Listen for local Alexa Echo SSDP discovery & events
  AlexaManager::getInstance().loop();

  // Poll Physical Input Buttons with debouncing
  ButtonManager::getInstance().loop();

  // Yield to FreeRTOS IDLE task to satisfy watchdog
  delay(1);
}
