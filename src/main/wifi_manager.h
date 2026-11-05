#ifndef SMARTHOME_WIFI_MANAGER_H
#define SMARTHOME_WIFI_MANAGER_H

#include <Arduino.h>
#include "config.h"

struct WifiStatus {
  bool sta_connected;
  String sta_ssid;
  String sta_ip;
  int sta_rssi;
  String ap_ssid;
  String ap_ip;
  String mac;
};

class WifiManager {
public:
  static WifiManager& getInstance();

  void begin();
  void loop();

  bool isStaConnected() const { return _staConnected; }
  WifiStatus getStatus();
  String getStatusJson();

  // Scan available networks, returns JSON array
  String scanNetworksJson();

  // Save new STA configuration and attempt reconnection
  bool setStaCredentials(const String& ssid, const String& password);

  String getApSsid() const { return _apSsid; }
  String getApIp() const { return _apIp; }
  String getStaIp() const { return _staIp; }

private:
  WifiManager();
  ~WifiManager() = default;

  void loadSettings();
  void setupCombinationMode();

  bool _initialized;
  bool _staConnected;
  String _staSsid;
  String _staPassword;
  String _apSsid;
  String _apPassword;
  String _apIp;
  String _staIp;
  unsigned long _lastReconnectAttempt;
  int _retryCount;
};

#endif // SMARTHOME_WIFI_MANAGER_H
