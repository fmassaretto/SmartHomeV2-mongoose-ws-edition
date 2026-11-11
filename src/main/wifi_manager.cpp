#include "wifi_manager.h"
#include <WiFi.h>
#include <Preferences.h>
#include <esp_wifi.h>

static Preferences s_wifiPrefs;

WifiManager& WifiManager::getInstance() {
  static WifiManager instance;
  return instance;
}

WifiManager::WifiManager()
    : _initialized(false),
      _staConnected(false),
      _staSsid(""),
      _staPassword(""),
      _apSsid(""),
      _apPassword(DEFAULT_AP_PASS),
      _apIp("192.168.4.1"),
      _staIp("0.0.0.0"),
      _lastReconnectAttempt(0),
      _retryCount(0) {}

void WifiManager::loadSettings() {
  s_wifiPrefs.begin("smarthome_wifi", false);
  _staSsid = s_wifiPrefs.getString("sta_ssid", DEFAULT_STA_SSID);
  _staPassword = s_wifiPrefs.getString("sta_pass", DEFAULT_STA_PASS);
  _apPassword = s_wifiPrefs.getString("ap_pass", DEFAULT_AP_PASS);
  s_wifiPrefs.end();

  // Generate unique AP SSID based on MAC address
  uint8_t mac[6];
  esp_wifi_get_mac(WIFI_IF_AP, mac);
  char apName[32];
  snprintf(apName, sizeof(apName), "%s%02X%02X", DEFAULT_AP_SSID_PREFIX, mac[4], mac[5]);
  _apSsid = String(apName);
}

void WifiManager::setupCombinationMode() {
  Serial.println("[WIFI] Initializing Combination Mode (AP + STA)...");

  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);

  // Set Dual Mode
  WiFi.mode(WIFI_AP_STA);

  // Set TX power to 15 dBm to prevent current spikes that cause voltage drops, UART framing errors & WPA2 drops
  WiFi.setTxPower(WIFI_POWER_15dBm);

  // Completely disable modem sleep to keep APB peripheral clock locked at 80MHz (eliminates UART baud glitches)
  esp_wifi_set_ps(WIFI_PS_NONE);
  WiFi.setSleep(false);

  // Start SoftAP
  IPAddress apIP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);

  WiFi.softAPConfig(apIP, gateway, subnet);
  bool apOk = WiFi.softAP(_apSsid.c_str(), _apPassword.c_str(), DEFAULT_AP_CHANNEL, 0, DEFAULT_AP_MAX_CONN);
  if (apOk) {
    _apIp = WiFi.softAPIP().toString();
    Serial.printf("[WIFI] Access Point started: SSID='%s', Pass='%s', IP=%s\n",
                  _apSsid.c_str(), _apPassword.c_str(), _apIp.c_str());
  } else {
    Serial.println("[WIFI] SoftAP failed to start!");
  }

  // Connect to STA if SSID is configured
  if (_staSsid.length() > 0) {
    Serial.printf("[WIFI] Connecting to Station SSID: '%s'...\n", _staSsid.c_str());
    WiFi.begin(_staSsid.c_str(), _staPassword.c_str());
  } else {
    Serial.println("[WIFI] No Station SSID configured. Running in AP mode waiting for setup.");
  }
}

void WifiManager::begin() {
  if (_initialized) return;
  loadSettings();
  setupCombinationMode();
  _initialized = true;
}

void WifiManager::loop() {
  // Check STA connection status non-blockingly
  wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED) {
    if (!_staConnected) {
      _staConnected = true;
      _staIp = WiFi.localIP().toString();
      _retryCount = 0;

      // Lock APB clock and prevent power sags after connection established
      esp_wifi_set_ps(WIFI_PS_NONE);
      WiFi.setSleep(false);
      WiFi.setTxPower(WIFI_POWER_15dBm);

      Serial.println("\n==================================================");
      Serial.printf("[WIFI] Station Connected! IP: %s (RSSI: %d dBm)\n",
                    _staIp.c_str(), WiFi.RSSI());
      Serial.printf(" [READY] Web UI Dashboard: http://%s\n", _staIp.c_str());
      Serial.println("==================================================\n");
    }
  } else {
    if (_staConnected) {
      _staConnected = false;
      _staIp = "0.0.0.0";
      Serial.println("[WIFI] Station disconnected from AP!");
    }

    // Auto reconnect every 5 seconds if configured
    if (_staSsid.length() > 0) {
      unsigned long now = millis();
      if (now - _lastReconnectAttempt > 5000) {
        _lastReconnectAttempt = now;
        _retryCount++;
        Serial.printf("[WIFI] Reconnecting to STA '%s' (attempt %d)...\n",
                      _staSsid.c_str(), _retryCount);
        WiFi.begin(_staSsid.c_str(), _staPassword.c_str());
      }
    }
  }
}

WifiStatus WifiManager::getStatus() {
  WifiStatus s;
  s.sta_connected = _staConnected;
  s.sta_ssid = _staSsid;
  s.sta_ip = _staIp;
  s.sta_rssi = _staConnected ? WiFi.RSSI() : 0;
  s.ap_ssid = _apSsid;
  s.ap_ip = _apIp;
  s.mac = WiFi.macAddress();
  return s;
}

String WifiManager::getStatusJson() {
  WifiStatus s = getStatus();
  String json = "{";
  json += "\"sta_connected\":" + String(s.sta_connected ? "true" : "false") + ",";
  json += "\"sta_ssid\":\"" + s.sta_ssid + "\",";
  json += "\"sta_ip\":\"" + s.sta_ip + "\",";
  json += "\"sta_rssi\":" + String(s.sta_rssi) + ",";
  json += "\"ap_ssid\":\"" + s.ap_ssid + "\",";
  json += "\"ap_ip\":\"" + s.ap_ip + "\",";
  json += "\"mac\":\"" + s.mac + "\"";
  json += "}";
  return json;
}

String WifiManager::scanNetworksJson() {
  Serial.println("[WIFI] Scanning networks...");
  int n = WiFi.scanNetworks(false, true); // non-async, passive channel scan
  String json = "[";
  for (int i = 0; i < n; ++i) {
    if (i > 0) json += ",";
    json += "{";
    json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
    json += "\"channel\":" + String(WiFi.channel(i)) + ",";
    json += "\"secure\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false");
    json += "}";
  }
  json += "]";
  WiFi.scanDelete();
  return json;
}

bool WifiManager::setStaCredentials(const String& ssid, const String& password) {
  if (ssid.length() == 0) return false;

  Serial.printf("[WIFI] Saving new STA credentials: SSID='%s'\n", ssid.c_str());

  s_wifiPrefs.begin("smarthome_wifi", false);
  s_wifiPrefs.putString("sta_ssid", ssid);
  s_wifiPrefs.putString("sta_pass", password);
  s_wifiPrefs.end();

  _staSsid = ssid;
  _staPassword = password;
  _staConnected = false;
  _retryCount = 0;
  _lastReconnectAttempt = millis();

  WiFi.disconnect();
  WiFi.begin(_staSsid.c_str(), _staPassword.c_str());
  return true;
}
