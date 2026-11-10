#ifndef SMARTHOME_ALEXA_MANAGER_H
#define SMARTHOME_ALEXA_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <lwip/sockets.h>
#include "../mongoose/mongoose.h"
#include "device_manager.h"

class AlexaManager {
public:
  static AlexaManager& getInstance();

  void begin();
  void loop();

  // HTTP handling for Philips Hue emulation (Port 80)
  bool isAlexaRequest(struct mg_http_message *hm);
  void handleHttpRequest(struct mg_connection *c, struct mg_http_message *hm);

private:
  AlexaManager();
  ~AlexaManager() = default;

  void startUdp();
  void stopUdp();
  void respondToSearch(IPAddress remoteIp, uint16_t remotePort, const char* searchTarget);
  void sendNotify();

  String getDescriptionXml();
  String getLightsJson();
  String getNewLightsJson();
  String getLightJson(int lightId);
  String getConfigJson();
  String getFullStateJson();
  String buildDeviceHueJson(const Device& dev, int lightId);

  bool handleSetLightState(int lightId, bool hasOn, bool onVal, bool hasBri, int briVal);

  int _udpSock;
  unsigned long _lastNotify;
  String _macClean;
  String _macFormatted;
};

#endif // SMARTHOME_ALEXA_MANAGER_H
