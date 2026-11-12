#include "alexa_manager.h"
#include <errno.h>
#include <MD5Builder.h>

AlexaManager& AlexaManager::getInstance() {
  static AlexaManager instance;
  return instance;
}

AlexaManager::AlexaManager()
    : _udpSock(-1), _lastNotify(0) {
}

void AlexaManager::begin() {
  // Disable WiFi sleep to prevent dropping multicast discovery packets
  WiFi.setSleep(false);

  _macFormatted = WiFi.macAddress();
  _macClean = _macFormatted;
  _macClean.replace(":", "");
  _macClean.toLowerCase();

  Serial.println("[ALEXA] Initialized Local Voice Assistant Bridge (Philips Hue V1 Emulation)");
  Serial.printf("[ALEXA] Bridge MAC: %s (ID: %s)\n", _macFormatted.c_str(), _macClean.c_str());
}

void AlexaManager::startUdp() {
  if (_udpSock >= 0) return;

  _udpSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
  if (_udpSock < 0) {
    Serial.printf("[ALEXA-SSDP] Failed to create UDP socket: %d\n", errno);
    return;
  }

  int yes = 1;
  setsockopt(_udpSock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

  struct sockaddr_in bindAddr;
  memset(&bindAddr, 0, sizeof(bindAddr));
  bindAddr.sin_family = AF_INET;
  bindAddr.sin_port = htons(1900);
  bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(_udpSock, (struct sockaddr *)&bindAddr, sizeof(bindAddr)) < 0) {
    Serial.printf("[ALEXA-SSDP] Failed to bind to 1900: %d\n", errno);
    close(_udpSock);
    _udpSock = -1;
    return;
  }

  // Join multicast group on Station interface IP specifically
  struct ip_mreq mreq;
  mreq.imr_multiaddr.s_addr = inet_addr("239.255.255.250");
  mreq.imr_interface.s_addr = (in_addr_t)WiFi.localIP();
  if (setsockopt(_udpSock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
    Serial.printf("[ALEXA-SSDP] Warning: Failed to join multicast on STA IP %s\n", WiFi.localIP().toString().c_str());
  }

  // Also join with INADDR_ANY
  mreq.imr_interface.s_addr = htonl(INADDR_ANY);
  setsockopt(_udpSock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));

  fcntl(_udpSock, F_SETFL, O_NONBLOCK);
  Serial.printf("[ALEXA-SSDP] Multicast listener active on 239.255.255.250:1900 (STA IP: %s)\n",
                WiFi.localIP().toString().c_str());

  sendNotify();
}

void AlexaManager::stopUdp() {
  if (_udpSock >= 0) {
    close(_udpSock);
    _udpSock = -1;
    Serial.println("[ALEXA-SSDP] Multicast listener stopped");
  }
}

void AlexaManager::loop() {
  if (WiFi.status() != WL_CONNECTED) {
    if (_udpSock >= 0) {
      stopUdp();
    }
    return;
  }

  if (_udpSock < 0) {
    startUdp();
  }

  if (_udpSock >= 0) {
    char packetBuffer[1024];
    struct sockaddr_in fromAddr;
    socklen_t fromLen = sizeof(fromAddr);
    int len = recvfrom(_udpSock, packetBuffer, sizeof(packetBuffer) - 1, 0, (struct sockaddr *)&fromAddr, &fromLen);
    if (len > 0) {
      packetBuffer[len] = '\0';
      IPAddress remoteIp(fromAddr.sin_addr.s_addr);
      uint16_t remotePort = ntohs(fromAddr.sin_port);

      if (strstr(packetBuffer, "M-SEARCH") != nullptr) {
        Serial.printf("[ALEXA-SSDP] M-SEARCH received from %s:%d\n", remoteIp.toString().c_str(), remotePort);
        if (strstr(packetBuffer, "ssdp:all") != nullptr) {
          respondToSearch(remoteIp, remotePort, "upnp:rootdevice");
          respondToSearch(remoteIp, remotePort, "urn:schemas-upnp-org:device:basic:1");
        } else if (strstr(packetBuffer, "upnp:rootdevice") != nullptr) {
          respondToSearch(remoteIp, remotePort, "upnp:rootdevice");
        } else if (strstr(packetBuffer, "device:basic:1") != nullptr || strstr(packetBuffer, "device:Basic:1") != nullptr) {
          respondToSearch(remoteIp, remotePort, "urn:schemas-upnp-org:device:basic:1");
        }
      }
    }
  }

  // Periodic SSDP alive announcement every 50 seconds to keep Echo cache fresh
  unsigned long now = millis();
  if (now - _lastNotify > 50000) {
    _lastNotify = now;
    sendNotify();
  }
}

void AlexaManager::respondToSearch(IPAddress remoteIp, uint16_t remotePort, const char* searchTarget) {
  if (_udpSock < 0) return;

  String ip = WiFi.localIP().toString();
  String bridgeId = _macClean.substring(0, 6) + "fffe" + _macClean.substring(6);
  bridgeId.toUpperCase();

  String reply = "HTTP/1.1 200 OK\r\n";
  reply += "HOST: 239.255.255.250:1900\r\n";
  reply += "EXT:\r\n";
  reply += "CACHE-CONTROL: max-age=100\r\n";
  reply += "LOCATION: http://" + ip + ":80/description.xml\r\n";
  reply += "SERVER: FreeRTOS/6.0.5, UPnP/1.0, IpBridge/1.17.0\r\n";
  reply += "hue-bridgeid: " + bridgeId + "\r\n";
  reply += "ST: " + String(searchTarget) + "\r\n";
  reply += "USN: uuid:2f402f80-da50-11e1-9b23-" + _macClean + "::" + String(searchTarget) + "\r\n\r\n";

  struct sockaddr_in toAddr;
  memset(&toAddr, 0, sizeof(toAddr));
  toAddr.sin_family = AF_INET;
  toAddr.sin_port = htons(remotePort);
  toAddr.sin_addr.s_addr = (in_addr_t)remoteIp;
  sendto(_udpSock, reply.c_str(), reply.length(), 0, (struct sockaddr *)&toAddr, sizeof(toAddr));

  Serial.printf("[ALEXA-SSDP] Replied 200 OK (%s) to %s:%d\n", searchTarget, remoteIp.toString().c_str(), remotePort);
}

void AlexaManager::sendNotify() {
  if (_udpSock < 0 || WiFi.status() != WL_CONNECTED) return;
  String ip = WiFi.localIP().toString();
  String bridgeId = _macClean.substring(0, 6) + "fffe" + _macClean.substring(6);
  bridgeId.toUpperCase();

  String notify = "NOTIFY * HTTP/1.1\r\n";
  notify += "HOST: 239.255.255.250:1900\r\n";
  notify += "CACHE-CONTROL: max-age=100\r\n";
  notify += "LOCATION: http://" + ip + ":80/description.xml\r\n";
  notify += "SERVER: FreeRTOS/6.0.5, UPnP/1.0, IpBridge/1.17.0\r\n";
  notify += "hue-bridgeid: " + bridgeId + "\r\n";
  notify += "NT: upnp:rootdevice\r\n";
  notify += "NTS: ssdp:alive\r\n";
  notify += "USN: uuid:2f402f80-da50-11e1-9b23-" + _macClean + "::upnp:rootdevice\r\n\r\n";

  struct sockaddr_in mcastAddr;
  memset(&mcastAddr, 0, sizeof(mcastAddr));
  mcastAddr.sin_family = AF_INET;
  mcastAddr.sin_port = htons(1900);
  mcastAddr.sin_addr.s_addr = inet_addr("239.255.255.250");
  sendto(_udpSock, notify.c_str(), notify.length(), 0, (struct sockaddr *)&mcastAddr, sizeof(mcastAddr));
}

bool AlexaManager::isAlexaRequest(struct mg_http_message *hm) {
  if (mg_match(hm->uri, mg_str("/description.xml"), NULL)) return true;
  if (mg_match(hm->uri, mg_str("/api"), NULL) || mg_match(hm->uri, mg_str("/api/"), NULL)) return true;
  if (mg_match(hm->uri, mg_str("/api/lights#"), NULL)) return true;

  if (mg_match(hm->uri, mg_str("/api/#"), NULL)) {
    // Exclude all known Web UI and Android App endpoints
    if (mg_match(hm->uri, mg_str("/api/auth#"), NULL) ||
        mg_match(hm->uri, mg_str("/api/devices#"), NULL) ||
        mg_match(hm->uri, mg_str("/api/buttons#"), NULL) ||
        mg_match(hm->uri, mg_str("/api/users#"), NULL) ||
        mg_match(hm->uri, mg_str("/api/rooms#"), NULL) ||
        mg_match(hm->uri, mg_str("/api/wifi#"), NULL) ||
        mg_match(hm->uri, mg_str("/api/system#"), NULL) ||
        mg_match(hm->uri, mg_str("/api/ota#"), NULL)) {
      return false;
    }
    return true; // Any other /api/* query is for Alexa / Hue API
  }

  return false;
}

void AlexaManager::handleHttpRequest(struct mg_connection *c, struct mg_http_message *hm) {
  // Only log state mutations (PUT) or pairing/discovery to avoid UART buffer overflows during background polling
  if (strncasecmp(hm->method.buf, "PUT", 3) == 0 ||
      mg_match(hm->uri, mg_str("/description.xml"), NULL) ||
      mg_match(hm->uri, mg_str("/api"), NULL) ||
      mg_match(hm->uri, mg_str("/api/"), NULL) ||
      mg_match(hm->uri, mg_str("/api/*/lights/new#"), NULL)) {
    Serial.printf("[ALEXA-HTTP] %.*s %.*s\n", (int)hm->method.len, hm->method.buf, (int)hm->uri.len, hm->uri.buf);
  }

  // 1. UPnP Description XML
  if (mg_match(hm->uri, mg_str("/description.xml"), NULL)) {
    String xml = getDescriptionXml();
    mg_http_reply(c, 200, "Content-Type: application/xml\r\nConnection: close\r\n", "%s", xml.c_str());
    Serial.println("[ALEXA-HTTP] Served /description.xml to Echo");
    return;
  }

  // 2. User Registration / Root Probe
  if (mg_match(hm->uri, mg_str("/api"), NULL) || mg_match(hm->uri, mg_str("/api/"), NULL)) {
    if (strncasecmp(hm->method.buf, "POST", 4) == 0) {
      Serial.println("[ALEXA-HTTP] Echo client paired via POST /api");
      mg_http_reply(c, 200, "Content-Type: application/json\r\nConnection: close\r\n",
                    "[{\"success\":{\"username\":\"echo_user\"}}]\n");
      return;
    } else {
      String json = getFullStateJson();
      mg_http_reply(c, 200, "Content-Type: application/json\r\nConnection: close\r\n", "%s", json.c_str());
      return;
    }
  }

  struct mg_str caps[5];

  // 3. Control light state: PUT /api/*/lights/*/state or /api/*/lights/*/state/
  if (mg_match(hm->uri, mg_str("/api/*/lights/*/state"), caps) ||
      mg_match(hm->uri, mg_str("/api/*/lights/*/state/"), caps)) {
    String idStr(caps[1].buf, caps[1].len);
    int lightId = idStr.toInt();

    const auto& devices = DeviceManager::getInstance().getDevices();
    int currentBri = 254;
    if (lightId >= 1 && lightId <= (int)devices.size()) {
      int val = devices[lightId - 1].value;
      if (val > 0) currentBri = (int)round(val * 254.0 / 100.0);
    }

    bool onVal = false;
    bool hasOn = mg_json_get_bool(hm->body, "$.on", &onVal);

    double briVal = -1;
    bool hasBri = mg_json_get_num(hm->body, "$.bri", &briVal);

    double briIncVal = 0;
    bool hasBriInc = mg_json_get_num(hm->body, "$.bri_inc", &briIncVal);
    if (hasBriInc) {
      hasBri = true;
      briVal = currentBri + briIncVal;
      if (briVal < 1) briVal = 1;
      if (briVal > 254) briVal = 254;
    }

    bool success = handleSetLightState(lightId, hasOn, onVal, hasBri, (int)briVal);

    String resp = "[";
    bool first = true;
    if (hasOn) {
      resp += "{\"success\":{\"/lights/" + idStr + "/state/on\":" + (onVal ? "true" : "false") + "}}";
      first = false;
    }
    if (hasBri && !hasBriInc) {
      if (!first) resp += ",";
      resp += "{\"success\":{\"/lights/" + idStr + "/state/bri\":" + String((int)briVal) + "}}";
      first = false;
    }
    if (hasBriInc) {
      if (!first) resp += ",";
      resp += "{\"success\":{\"/lights/" + idStr + "/state/bri_inc\":" + String((int)briIncVal) + "}}";
      first = false;
    }
    if (!hasOn && !hasBri) {
      resp += "{\"success\":{\"/lights/" + idStr + "/state/on\":" + (success ? "true" : "false") + "}}";
    }
    resp += "]\n";

    mg_http_reply(c, 200, "Content-Type: application/json\r\nConnection: close\r\n", "%s", resp.c_str());
    return;
  }

  // 3.5 New lights query: GET /api/*/lights/new
  if (mg_match(hm->uri, mg_str("/api/*/lights/new"), NULL) ||
      mg_match(hm->uri, mg_str("/api/*/lights/new/"), NULL) ||
      mg_match(hm->uri, mg_str("/api/lights/new"), NULL) ||
      mg_match(hm->uri, mg_str("/api/lights/new/"), NULL)) {
    String json = getNewLightsJson() + "\n";
    mg_http_reply(c, 200, "Content-Type: application/json\r\nConnection: close\r\n", "%s", json.c_str());
    Serial.printf("[ALEXA-HTTP] Served /lights/new: %s\n", json.c_str());
    return;
  }

  // 3.6 Search for new lights: POST /api/*/lights
  if ((mg_match(hm->uri, mg_str("/api/*/lights"), NULL) ||
       mg_match(hm->uri, mg_str("/api/*/lights/"), NULL) ||
       mg_match(hm->uri, mg_str("/api/lights"), NULL) ||
       mg_match(hm->uri, mg_str("/api/lights/"), NULL)) &&
      strncasecmp(hm->method.buf, "POST", 4) == 0) {
    Serial.println("[ALEXA-HTTP] Search for new lights initiated via POST /lights");
    mg_http_reply(c, 200, "Content-Type: application/json\r\nConnection: close\r\n",
                  "[{\"success\":{\"/lights\":\"Searching for new devices\"}}]\n");
    return;
  }

  // 4. Single light query: GET /api/*/lights/* or /api/*/lights/*/
  if (mg_match(hm->uri, mg_str("/api/*/lights/*"), caps) ||
      mg_match(hm->uri, mg_str("/api/*/lights/*/"), caps)) {
    String idStr(caps[1].buf, caps[1].len);
    int lightId = idStr.toInt();
    String json = getLightJson(lightId) + "\n";
    mg_http_reply(c, 200, "Content-Type: application/json\r\nConnection: close\r\n", "%s", json.c_str());
    return;
  }

  // 5. All lights query: GET /api/*/lights or GET /api/lights
  if (mg_match(hm->uri, mg_str("/api/*/lights"), NULL) ||
      mg_match(hm->uri, mg_str("/api/*/lights/"), NULL) ||
      mg_match(hm->uri, mg_str("/api/lights"), NULL) ||
      mg_match(hm->uri, mg_str("/api/lights/"), NULL)) {
    String json = getLightsJson() + "\n";
    mg_http_reply(c, 200, "Content-Type: application/json\r\nConnection: close\r\n", "%s", json.c_str());
    return;
  }

  // 6. Bridge config query: GET /api/*/config
  if (mg_match(hm->uri, mg_str("/api/*/config"), NULL) ||
      mg_match(hm->uri, mg_str("/api/*/config/"), NULL) ||
      mg_match(hm->uri, mg_str("/api/config"), NULL) ||
      mg_match(hm->uri, mg_str("/api/config/"), NULL)) {
    String json = getConfigJson() + "\n";
    mg_http_reply(c, 200, "Content-Type: application/json\r\nConnection: close\r\n", "%s", json.c_str());
    return;
  }

  // 7. Full bridge state dump: GET /api/* or GET /api/*/
  if ((mg_match(hm->uri, mg_str("/api/*"), NULL) || mg_match(hm->uri, mg_str("/api/*/"), NULL)) &&
      strncasecmp(hm->method.buf, "GET", 3) == 0) {
    String json = getFullStateJson() + "\n";
    mg_http_reply(c, 200, "Content-Type: application/json\r\nConnection: close\r\n", "%s", json.c_str());
    return;
  }

  // Fallback for groups, scenes, sensors, schedules, rules
  mg_http_reply(c, 200, "Content-Type: application/json\r\nConnection: close\r\n", "{}\n");
}

bool AlexaManager::handleSetLightState(int lightId, bool hasOn, bool onVal, bool hasBri, int briVal) {
  const auto& devices = DeviceManager::getInstance().getDevices();
  if (lightId < 1 || lightId > (int)devices.size()) {
    return false;
  }
  const Device& dev = devices[lightId - 1];
  if (!dev.alexaEnabled) {
    Serial.printf("[ALEXA] Voice action rejected: '%s' (#%d) is not enabled for Alexa\n",
                  dev.name.c_str(), lightId);
    return false;
  }

  // Determine state (ON / OFF)
  bool targetState = dev.state;
  if (hasOn) {
    targetState = onVal;
  }
  if (hasBri) {
    if (briVal == 0) {
      targetState = false;
    } else {
      targetState = true;
    }
  }

  // Determine target percentage (1 - 100%)
  int targetValue = dev.value;
  if (hasBri) {
    targetValue = (int)round((briVal * 100.0) / 254.0);
    if (targetValue < 1 && targetState) targetValue = 1;
    if (targetValue > 100) targetValue = 100;
  } else if (targetState && targetValue <= 0) {
    // When turning ON without explicit brightness, default to 100%
    targetValue = 100;
  }

  Serial.printf("[ALEXA] Voice action for '%s' (#%d): state=%s, value=%d%%\n",
                dev.name.c_str(), lightId, targetState ? "ON" : "OFF", targetValue);

  return DeviceManager::getInstance().setDeviceState(dev.id, targetState, targetValue);
}

String AlexaManager::getDescriptionXml() {
  String ip = WiFi.localIP().toString();
  String xml = "<?xml version=\"1.0\"?>\r\n";
  xml += "<root xmlns=\"urn:schemas-upnp-org:device-1-0\">\r\n";
  xml += "<specVersion><major>1</major><minor>0</minor></specVersion>\r\n";
  xml += "<URLBase>http://" + ip + ":80/</URLBase>\r\n";
  xml += "<device>\r\n";
  xml += "<deviceType>urn:schemas-upnp-org:device:Basic:1</deviceType>\r\n";
  xml += "<friendlyName>Philips hue (" + ip + ")</friendlyName>\r\n";
  xml += "<manufacturer>Royal Philips Electronics</manufacturer>\r\n";
  xml += "<manufacturerURL>http://www.philips.com</manufacturerURL>\r\n";
  xml += "<modelDescription>Philips hue Personal Wireless Lighting</modelDescription>\r\n";
  xml += "<modelName>Philips hue bridge 2012</modelName>\r\n";
  xml += "<modelNumber>929000226503</modelNumber>\r\n";
  xml += "<modelURL>http://www.meethue.com</modelURL>\r\n";
  xml += "<serialNumber>" + _macClean + "</serialNumber>\r\n";
  xml += "<UDN>uuid:2f402f80-da50-11e1-9b23-" + _macClean + "</UDN>\r\n";
  xml += "</device>\r\n";
  xml += "</root>\r\n";
  return xml;
}

String AlexaManager::buildDeviceHueJson(const Device& dev, int lightId) {
  int val = dev.value;
  if (dev.state && val <= 0) val = 100;

  int bri = (int)round((val * 254.0) / 100.0);
  if (bri < 1) bri = (dev.state ? 254 : 1);
  if (bri > 254) bri = 254;

  // Generate completely unique, persistent 8-octet Zigbee MAC for each device: 00:17:88:XX:XX:XX:XX:XX-0B
  MD5Builder md5;
  md5.begin();
  md5.add(_macClean);
  md5.add(dev.id);
  md5.add(String(lightId));
  md5.calculate();
  String hash = md5.toString();
  hash.toUpperCase();

  String uid = "00:17:88:" + hash.substring(0, 2) + ":" + hash.substring(2, 4) + ":" +
               hash.substring(4, 6) + ":" + hash.substring(6, 8) + ":" + hash.substring(8, 10) + "-0B";

  String safeName = dev.name;
  safeName.replace("\"", "\\\"");

  String json = "{";
  json += "\"state\":{";
  json += "\"on\":" + String(dev.state ? "true" : "false") + ",";
  json += "\"bri\":" + String(bri) + ",";
  json += "\"alert\":\"none\",";
  json += "\"mode\":\"homeautomation\",";
  json += "\"reachable\":true";
  json += "},";
  json += "\"type\":\"Dimmable light\",";
  json += "\"name\":\"" + safeName + "\",";
  json += "\"modelid\":\"LWB010\",";
  json += "\"manufacturername\":\"Philips\",";
  json += "\"productname\":\"E27\",";
  json += "\"uniqueid\":\"" + uid + "\",";
  json += "\"swversion\":\"1.29.0_r21169\"";
  json += "}";
  return json;
}

String AlexaManager::getNewLightsJson() {
  const auto& devices = DeviceManager::getInstance().getDevices();
  String json = "{";
  for (size_t i = 0; i < devices.size(); i++) {
    if (!devices[i].alexaEnabled) continue;
    String safeName = devices[i].name;
    safeName.replace("\"", "\\\"");
    json += "\"" + String(i + 1) + "\":{\"name\":\"" + safeName + "\"},";
  }
  json += "\"lastscan\":\"active\"}";
  return json;
}

String AlexaManager::getLightJson(int lightId) {
  const auto& devices = DeviceManager::getInstance().getDevices();
  if (lightId < 1 || lightId > (int)devices.size()) {
    return "{}";
  }
  const Device& dev = devices[lightId - 1];
  if (!dev.alexaEnabled) {
    return "{}";
  }
  return buildDeviceHueJson(dev, lightId);
}

String AlexaManager::getLightsJson() {
  const auto& devices = DeviceManager::getInstance().getDevices();
  String json = "{";
  bool first = true;
  for (size_t i = 0; i < devices.size(); i++) {
    if (!devices[i].alexaEnabled) continue;
    if (!first) json += ",";
    first = false;
    json += "\"" + String(i + 1) + "\":" + buildDeviceHueJson(devices[i], (int)(i + 1));
  }
  json += "}";
  return json;
}

String AlexaManager::getConfigJson() {
  String ip = WiFi.localIP().toString();
  String bridgeId = _macClean.substring(0, 6) + "fffe" + _macClean.substring(6);
  bridgeId.toUpperCase();

  String json = "{";
  json += "\"name\":\"Philips hue\",";
  json += "\"bridgeid\":\"" + bridgeId + "\",";
  json += "\"modelid\":\"BSB002\",";
  json += "\"mac\":\"" + _macFormatted + "\",";
  json += "\"dhcp\":true,";
  json += "\"ipaddress\":\"" + ip + "\",";
  json += "\"netmask\":\"" + WiFi.subnetMask().toString() + "\",";
  json += "\"gateway\":\"" + WiFi.gatewayIP().toString() + "\",";
  json += "\"tcp_port\":80,";
  json += "\"swversion\":\"01036562\",";
  json += "\"apiversion\":\"1.16.0\",";
  json += "\"linkbutton\":false,";
  json += "\"portalservices\":false,";
  json += "\"portalconnection\":\"connected\"";
  json += "}";
  return json;
}

String AlexaManager::getFullStateJson() {
  String json = "{";
  json += "\"lights\":" + getLightsJson() + ",";
  json += "\"groups\":{},";
  json += "\"config\":" + getConfigJson() + ",";
  json += "\"schedules\":{},";
  json += "\"scenes\":{},";
  json += "\"rules\":{},";
  json += "\"sensors\":{}";
  json += "}";
  return json;
}
