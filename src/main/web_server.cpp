#include "web_server.h"
#include "web_ui.h"
#include "auth_manager.h"
#include "device_manager.h"
#include "wifi_manager.h"
#include "alexa_manager.h"
#include <esp_ota_ops.h>
#include <esp_system.h>

struct OtaState {
  char marker; // 'O'
  esp_ota_handle_t handle;
  const esp_partition_t* partition;
  size_t expected;
  size_t received;
  bool failed;
};

static String getJsonString(struct mg_str json, const char* path) {
  char* s = mg_json_get_str(json, path);
  if (!s) return "";
  String res(s);
  free(s);
  return res;
}

static String extractToken(struct mg_http_message *hm) {
  // Check Authorization: Bearer <token>
  struct mg_str *auth = mg_http_get_header(hm, "Authorization");
  if (auth && auth->len > 7 && strncasecmp(auth->buf, "Bearer ", 7) == 0) {
    return String(auth->buf + 7, auth->len - 7);
  }

  // Check Cookie: access_token=<token>
  struct mg_str *cookie = mg_http_get_header(hm, "Cookie");
  if (cookie) {
    String cStr(cookie->buf, cookie->len);
    int idx = cStr.indexOf("access_token=");
    if (idx >= 0) {
      int end = cStr.indexOf(';', idx);
      if (end == -1) end = cStr.length();
      return cStr.substring(idx + 13, end);
    }
  }
  return "";
}

// Universal JSON API response helper with complete CORS & Private Network Access (PNA) headers
static void sendJsonResponse(struct mg_connection *c, struct mg_http_message *hm, int status, const String& body, const char* extraHeaders = NULL) {
  struct mg_str *origin = mg_http_get_header(hm, "Origin");
  String headers = "Content-Type: application/json\r\n";
  if (origin && origin->len > 0) {
    headers += "Access-Control-Allow-Origin: " + String(origin->buf, origin->len) + "\r\n";
    headers += "Access-Control-Allow-Credentials: true\r\n";
  } else {
    headers += "Access-Control-Allow-Origin: *\r\n";
  }
  headers += "Access-Control-Allow-Private-Network: true\r\n";
  if (extraHeaders && strlen(extraHeaders) > 0) {
    headers += extraHeaders;
  }
  mg_http_reply(c, status, headers.c_str(), "%s", body.c_str());
}

WebServer& WebServer::getInstance() {
  static WebServer instance;
  return instance;
}

WebServer::WebServer() : _initialized(false) {}

void WebServer::begin() {
  if (_initialized) return;

  // Set Mongoose log level to INFO to avoid verbose iobuf allocation messages
  mg_log_set(MG_LL_INFO);
  mg_mgr_init(&_mgr);

  char url[32];
  snprintf(url, sizeof(url), "http://0.0.0.0:%d", HTTP_PORT);
  struct mg_connection *c = mg_http_listen(&_mgr, url, httpEventHandler, this);

  if (c) {
    Serial.printf("[HTTP] Mongoose Web Server listening on %s\n", url);
  } else {
    Serial.println("[HTTP] Failed to start Mongoose HTTP listener!");
  }

  // Hook device state changes to WebSocket broadcast
  DeviceManager::getInstance().setChangeCallback([this](const String& event, const String& payload) {
    this->broadcastWs(event, payload);
  });

  _initialized = true;
}

void WebServer::poll() {
  mg_mgr_poll(&_mgr, 10);
}

void WebServer::broadcastWs(const String& eventType, const String& payloadJson) {
  String msg = "{";
  msg += "\"event\":\"" + eventType + "\",";
  msg += "\"data\":" + payloadJson;
  msg += "}";

  for (struct mg_connection *c = _mgr.conns; c != NULL; c = c->next) {
    if (c->is_websocket) {
      mg_ws_send(c, msg.c_str(), msg.length(), WEBSOCKET_OP_TEXT);
    }
  }
}

void WebServer::httpEventHandler(struct mg_connection *c, int ev, void *ev_data) {
  WebServer* server = (WebServer*)c->fn_data;

  // 1. Chunked Streaming OTA Upload handler
  struct OtaState *ota = (struct OtaState *)c->data;
  if (ota->marker == 'O') {
    if (ev == MG_EV_READ && c->recv.len > 0) {
      size_t chunk = c->recv.len;
      if (ota->received + chunk > ota->expected) {
        chunk = ota->expected - ota->received;
      }

      if (!ota->failed && chunk > 0) {
        esp_err_t err = esp_ota_write(ota->handle, c->recv.buf, chunk);
        if (err != ESP_OK) {
          Serial.printf("[OTA] Write error: 0x%x\n", err);
          ota->failed = true;
        } else {
          ota->received += chunk;
        }
      }
      mg_iobuf_del(&c->recv, 0, c->recv.len);

      if (ota->received >= ota->expected) {
        if (!ota->failed) {
          esp_err_t endErr = esp_ota_end(ota->handle);
          if (endErr == ESP_OK) {
            esp_ota_set_boot_partition(ota->partition);
            Serial.println("[OTA] Firmware updated successfully! Rebooting in 1s...");
            mg_http_reply(c, 200, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Private-Network: true\r\n", "{\"success\":true}\n");
            
            // Schedule reboot
            mg_timer_add(&server->_mgr, 1000, 0, [](void*) {
              esp_restart();
            }, NULL);
          } else {
            Serial.printf("[OTA] End error: 0x%x\n", endErr);
            mg_http_reply(c, 500, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Private-Network: true\r\n", "{\"error\":\"OTA end failed\"}\n");
          }
        } else {
          mg_http_reply(c, 500, "Content-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Private-Network: true\r\n", "{\"error\":\"OTA write error\"}\n");
        }
        memset(ota, 0, sizeof(*ota));
        c->is_draining = 1;
      }
    } else if (ev == MG_EV_CLOSE) {
      if (ota->received < ota->expected && ota->handle) {
        esp_ota_end(ota->handle);
      }
      memset(ota, 0, sizeof(*ota));
    }
    return;
  }

  // 2. Incoming HTTP Header inspection (for OTA prep)
  if (ev == MG_EV_HTTP_HDRS) {
    struct mg_http_message *hm = (struct mg_http_message *)ev_data;
    if (mg_match(hm->uri, mg_str("/api/ota/upload"), NULL) && strncasecmp(hm->method.buf, "POST", 4) == 0) {
      String token = extractToken(hm);
      User user;
      if (!AuthManager::getInstance().validateToken(token, user) || !user.isAdmin()) {
        sendJsonResponse(c, hm, 403, "{\"error\":\"Admin required\"}\n");
        c->is_draining = 1;
        return;
      }

      struct mg_str *cl = mg_http_get_header(hm, "Content-Length");
      size_t totalLen = cl ? mg_str_to_num(*cl, 10, 0, 0) : 0;
      if (totalLen == 0) {
        sendJsonResponse(c, hm, 400, "{\"error\":\"Invalid Content-Length\"}\n");
        c->is_draining = 1;
        return;
      }

      const esp_partition_t *nextPart = esp_ota_get_next_update_partition(NULL);
      if (!nextPart) {
        sendJsonResponse(c, hm, 500, "{\"error\":\"No OTA partition\"}\n");
        c->is_draining = 1;
        return;
      }

      esp_ota_handle_t handle = 0;
      esp_err_t err = esp_ota_begin(nextPart, totalLen, &handle);
      if (err != ESP_OK) {
        Serial.printf("[OTA] Begin error: 0x%x\n", err);
        sendJsonResponse(c, hm, 500, "{\"error\":\"OTA begin failed\"}\n");
        c->is_draining = 1;
        return;
      }

      Serial.printf("[OTA] Starting OTA write to '%s' (%lu bytes)\n", nextPart->label, (unsigned long)totalLen);
      ota->marker = 'O';
      ota->handle = handle;
      ota->partition = nextPart;
      ota->expected = totalLen;
      ota->received = 0;
      ota->failed = false;

      // Consume any body data received with headers
      if (hm->body.len > 0) {
        esp_ota_write(handle, hm->body.buf, hm->body.len);
        ota->received += hm->body.len;
        mg_iobuf_del(&c->recv, 0, hm->head.len + hm->body.len);
      } else {
        mg_iobuf_del(&c->recv, 0, hm->head.len);
      }
      return;
    }
  }

  // 3. Full HTTP Request Messages
  if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *hm = (struct mg_http_message *)ev_data;

    // --- CORS & Private Network Access (PNA) Preflight OPTIONS Handler ---
    if (strncasecmp(hm->method.buf, "OPTIONS", 7) == 0) {
      struct mg_str *origin = mg_http_get_header(hm, "Origin");
      String originHeader = "Access-Control-Allow-Origin: *\r\n";
      if (origin && origin->len > 0) {
        originHeader = "Access-Control-Allow-Origin: " + String(origin->buf, origin->len) + "\r\n";
        originHeader += "Access-Control-Allow-Credentials: true\r\n";
      }

      String respHeaders = originHeader +
        "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type, Authorization, X-Requested-With, Origin, Accept\r\n"
        "Access-Control-Allow-Private-Network: true\r\n"
        "Access-Control-Max-Age: 86400\r\n";

      mg_http_reply(c, 204, respHeaders.c_str(), "");
      return;
    }

    // --- Static Web UI & Manifest ---
    if (mg_match(hm->uri, mg_str("/"), NULL) || mg_match(hm->uri, mg_str("/index.html"), NULL)) {
      // Pre-allocate buffer to prevent multiple incremental reallocations
      mg_iobuf_resize(&c->send, INDEX_HTML_GZ_LEN + 256);
      mg_printf(c, "HTTP/1.1 200 OK\r\n"
                   "Content-Type: text/html; charset=utf-8\r\n"
                   "Content-Encoding: gzip\r\n"
                   "Content-Length: %lu\r\n"
                   "Cache-Control: no-cache\r\n"
                   "Access-Control-Allow-Origin: *\r\n"
                   "Access-Control-Allow-Private-Network: true\r\n\r\n",
                (unsigned long)INDEX_HTML_GZ_LEN);
      mg_send(c, INDEX_HTML_GZ, INDEX_HTML_GZ_LEN);
      c->is_resp = 0;
      return;
    }

    if (mg_match(hm->uri, mg_str("/manifest.json"), NULL)) {
      mg_http_reply(c, 200,
                    "Content-Type: application/json\r\n"
                    "Cache-Control: max-age=86400\r\n"
                    "Access-Control-Allow-Origin: *\r\n"
                    "Access-Control-Allow-Private-Network: true\r\n",
                    "%s", MANIFEST_JSON);
      return;
    }

    // --- WebSocket Upgrade ---
    if (mg_match(hm->uri, mg_str("/ws"), NULL)) {
      mg_ws_upgrade(c, hm, NULL);
      Serial.printf("[WS] New client connected (ID: %lu)\n", c->id);
      return;
    }

    // --- Alexa / Local Voice Assistant Hue API Integration ---
    if (AlexaManager::getInstance().isAlexaRequest(hm)) {
      AlexaManager::getInstance().handleHttpRequest(c, hm);
      return;
    }

    // --- API: Auth Login ---
    if (mg_match(hm->uri, mg_str("/api/auth/login"), NULL)) {
      String u = getJsonString(hm->body, "$.username");
      String p = getJsonString(hm->body, "$.password");

      String token;
      User user;
      if (AuthManager::getInstance().login(u, p, token, user)) {
        String resp = "{";
        resp += "\"token\":\"" + token + "\",";
        resp += "\"user\":" + AuthManager::getInstance().getUserProfileJson(user);
        resp += "}\n";

        char cookie[128];
        snprintf(cookie, sizeof(cookie),
                 "Set-Cookie: access_token=%s; Path=/; HttpOnly; SameSite=Lax; Max-Age=604800\r\n",
                 token.c_str());

        sendJsonResponse(c, hm, 200, resp, cookie);
      } else {
        sendJsonResponse(c, hm, 401, "{\"error\":\"Invalid username or password\"}\n");
      }
      return;
    }

    // --- API: Auth Logout ---
    if (mg_match(hm->uri, mg_str("/api/auth/logout"), NULL)) {
      String token = extractToken(hm);
      AuthManager::getInstance().logout(token);
      sendJsonResponse(c, hm, 200, "{\"success\":true}\n", "Set-Cookie: access_token=; Path=/; Max-Age=0\r\n");
      return;
    }

    // --- Protected API Token Check ---
    String token = extractToken(hm);
    User currentUser;
    bool authed = AuthManager::getInstance().validateToken(token, currentUser);

    if (mg_match(hm->uri, mg_str("/api/#"), NULL)) {
      if (!authed) {
        sendJsonResponse(c, hm, 401, "{\"error\":\"Unauthorized - Please sign in\"}\n");
        return;
      }
    }

    // --- API: Get Current User Profile ---
    if (mg_match(hm->uri, mg_str("/api/auth/me"), NULL)) {
      String json = AuthManager::getInstance().getUserProfileJson(currentUser) + "\n";
      sendJsonResponse(c, hm, 200, json);
      return;
    }

    // --- API: Get Devices (Filtered by User Room Permissions!) ---
    if (mg_match(hm->uri, mg_str("/api/devices"), NULL) && strncasecmp(hm->method.buf, "GET", 3) == 0) {
      String json = DeviceManager::getInstance().getDevicesJsonForUser(currentUser) + "\n";
      sendJsonResponse(c, hm, 200, json);
      return;
    }

    // --- API: Control Device (Permission check enforced!) ---
    if (mg_match(hm->uri, mg_str("/api/devices/control"), NULL)) {
      String devId = getJsonString(hm->body, "$.deviceId");
      bool state = false;
      mg_json_get_bool(hm->body, "$.state", &state);
      double val = -1;
      mg_json_get_num(hm->body, "$.value", &val);

      Device* dev = DeviceManager::getInstance().getDevice(devId);
      if (!dev) {
        sendJsonResponse(c, hm, 404, "{\"error\":\"Device not found\"}\n");
        return;
      }

      // Check Device Permission!
      if (!DeviceManager::getInstance().canUserAccessDevice(currentUser, *dev)) {
        Serial.printf("[AUTH-DENIED] User '%s' blocked from switching device '%s' (allowed: '%s')\n",
                      currentUser.username.c_str(), dev->id.c_str(), dev->allowedUsers.c_str());
        sendJsonResponse(c, hm, 403, "{\"error\":\"Permission denied: You do not have access to this device\"}\n");
        return;
      }

      DeviceManager::getInstance().setDeviceState(devId, state, (int)val);
      sendJsonResponse(c, hm, 200, "{\"success\":true}\n");
      return;
    }

    // --- Admin APIs (Requires role == "admin") ---
    if (mg_match(hm->uri, mg_str("/api/users#"), NULL) ||
        mg_match(hm->uri, mg_str("/api/devices/add"), NULL) ||
        mg_match(hm->uri, mg_str("/api/devices/update"), NULL) ||
        mg_match(hm->uri, mg_str("/api/devices/delete"), NULL) ||
        mg_match(hm->uri, mg_str("/api/rooms#"), NULL) ||
        mg_match(hm->uri, mg_str("/api/wifi#"), NULL) ||
        mg_match(hm->uri, mg_str("/api/system#"), NULL)) {
      if (!currentUser.isAdmin()) {
        sendJsonResponse(c, hm, 403, "{\"error\":\"Admin privileges required\"}\n");
        return;
      }
    }

    // Add Device / Control
    if (mg_match(hm->uri, mg_str("/api/devices/add"), NULL)) {
      Device d;
      d.id = getJsonString(hm->body, "$.id");
      d.name = getJsonString(hm->body, "$.name");
      d.roomId = getJsonString(hm->body, "$.roomId");
      
      double typeVal = 0;
      mg_json_get_num(hm->body, "$.type", &typeVal);
      d.type = (DeviceType)(int)typeVal;

      double pinVal = -1;
      mg_json_get_num(hm->body, "$.pin", &pinVal);
      d.pin = (int)pinVal;

      bool inverted = false;
      mg_json_get_bool(hm->body, "$.inverted", &inverted);
      d.inverted = inverted;

      String allowed = getJsonString(hm->body, "$.allowedUsers");
      d.allowedUsers = allowed.length() ? allowed : "*";

      bool alexa = true;
      mg_json_get_bool(hm->body, "$.alexaEnabled", &alexa);
      d.alexaEnabled = alexa;

      d.pwmChannel = -1;
      d.state = false;
      d.value = (d.type == DEV_DIMMER) ? 75 : 0;

      String err;
      if (DeviceManager::getInstance().addDevice(d, err)) {
        sendJsonResponse(c, hm, 200, "{\"success\":true}\n");
      } else {
        sendJsonResponse(c, hm, 400, "{\"error\":\"" + err + "\"}\n");
      }
      return;
    }

    // Update Device / Control
    if (mg_match(hm->uri, mg_str("/api/devices/update"), NULL)) {
      Device d;
      d.id = getJsonString(hm->body, "$.id");
      d.name = getJsonString(hm->body, "$.name");
      d.roomId = getJsonString(hm->body, "$.roomId");

      double typeVal = 0;
      mg_json_get_num(hm->body, "$.type", &typeVal);
      d.type = (DeviceType)(int)typeVal;

      double pinVal = -1;
      mg_json_get_num(hm->body, "$.pin", &pinVal);
      d.pin = (int)pinVal;

      bool inverted = false;
      mg_json_get_bool(hm->body, "$.inverted", &inverted);
      d.inverted = inverted;

      String allowed = getJsonString(hm->body, "$.allowedUsers");
      d.allowedUsers = allowed.length() ? allowed : "*";

      bool alexa = true;
      mg_json_get_bool(hm->body, "$.alexaEnabled", &alexa);
      d.alexaEnabled = alexa;

      d.pwmChannel = -1;
      d.state = false;
      d.value = 0;

      String err;
      if (DeviceManager::getInstance().updateDevice(d, err)) {
        sendJsonResponse(c, hm, 200, "{\"success\":true}\n");
      } else {
        sendJsonResponse(c, hm, 400, "{\"error\":\"" + err + "\"}\n");
      }
      return;
    }

    // Delete Device / Control
    if (mg_match(hm->uri, mg_str("/api/devices/delete"), NULL)) {
      String devId = getJsonString(hm->body, "$.id");
      String err;
      if (DeviceManager::getInstance().deleteDevice(devId, err)) {
        sendJsonResponse(c, hm, 200, "{\"success\":true}\n");
      } else {
        sendJsonResponse(c, hm, 400, "{\"error\":\"" + err + "\"}\n");
      }
      return;
    }

    // Add Room
    if (mg_match(hm->uri, mg_str("/api/rooms/add"), NULL)) {
      Room r;
      r.id = getJsonString(hm->body, "$.id");
      r.name = getJsonString(hm->body, "$.name");
      r.icon = getJsonString(hm->body, "$.icon");
      if (r.icon.length() == 0) r.icon = "home";

      String err;
      if (DeviceManager::getInstance().addRoom(r, err)) {
        sendJsonResponse(c, hm, 200, "{\"success\":true}\n");
      } else {
        sendJsonResponse(c, hm, 400, "{\"error\":\"" + err + "\"}\n");
      }
      return;
    }

    // Delete Room
    if (mg_match(hm->uri, mg_str("/api/rooms/delete"), NULL)) {
      String roomId = getJsonString(hm->body, "$.id");
      String err;
      if (DeviceManager::getInstance().deleteRoom(roomId, err)) {
        sendJsonResponse(c, hm, 200, "{\"success\":true}\n");
      } else {
        sendJsonResponse(c, hm, 400, "{\"error\":\"" + err + "\"}\n");
      }
      return;
    }

    // Users List
    if (mg_match(hm->uri, mg_str("/api/users"), NULL) && strncasecmp(hm->method.buf, "GET", 3) == 0) {
      String json = AuthManager::getInstance().getUsersJson() + "\n";
      sendJsonResponse(c, hm, 200, json);
      return;
    }

    // Add User
    if (mg_match(hm->uri, mg_str("/api/users/add"), NULL)) {
      User u;
      u.username = getJsonString(hm->body, "$.username");
      u.password = getJsonString(hm->body, "$.password");
      u.displayName = getJsonString(hm->body, "$.displayName");
      u.role = getJsonString(hm->body, "$.role");
      u.allowedRooms = getJsonString(hm->body, "$.allowedRooms");

      String err;
      if (AuthManager::getInstance().addUser(u, err)) {
        sendJsonResponse(c, hm, 200, "{\"success\":true}\n");
      } else {
        sendJsonResponse(c, hm, 400, "{\"error\":\"" + err + "\"}\n");
      }
      return;
    }

    // Update User
    if (mg_match(hm->uri, mg_str("/api/users/update"), NULL)) {
      User u;
      u.username = getJsonString(hm->body, "$.username");
      u.password = getJsonString(hm->body, "$.password");
      u.displayName = getJsonString(hm->body, "$.displayName");
      u.role = getJsonString(hm->body, "$.role");
      u.allowedRooms = getJsonString(hm->body, "$.allowedRooms");

      String err;
      if (AuthManager::getInstance().updateUser(u, err)) {
        sendJsonResponse(c, hm, 200, "{\"success\":true}\n");
      } else {
        sendJsonResponse(c, hm, 400, "{\"error\":\"" + err + "\"}\n");
      }
      return;
    }

    // Delete User
    if (mg_match(hm->uri, mg_str("/api/users/delete"), NULL)) {
      String u = getJsonString(hm->body, "$.username");
      String err;
      if (AuthManager::getInstance().deleteUser(u, err)) {
        sendJsonResponse(c, hm, 200, "{\"success\":true}\n");
      } else {
        sendJsonResponse(c, hm, 400, "{\"error\":\"" + err + "\"}\n");
      }
      return;
    }

    // WiFi Status
    if (mg_match(hm->uri, mg_str("/api/wifi/status"), NULL)) {
      String json = WifiManager::getInstance().getStatusJson() + "\n";
      sendJsonResponse(c, hm, 200, json);
      return;
    }

    // WiFi Scan
    if (mg_match(hm->uri, mg_str("/api/wifi/scan"), NULL)) {
      String json = WifiManager::getInstance().scanNetworksJson() + "\n";
      sendJsonResponse(c, hm, 200, json);
      return;
    }

    // WiFi Config
    if (mg_match(hm->uri, mg_str("/api/wifi/config"), NULL)) {
      String ssid = getJsonString(hm->body, "$.ssid");
      String pass = getJsonString(hm->body, "$.password");
      if (WifiManager::getInstance().setStaCredentials(ssid, pass)) {
        sendJsonResponse(c, hm, 200, "{\"success\":true}\n");
      } else {
        sendJsonResponse(c, hm, 400, "{\"error\":\"Invalid SSID\"}\n");
      }
      return;
    }

    // System Stats
    if (mg_match(hm->uri, mg_str("/api/system/stats"), NULL)) {
      String json = DeviceManager::getInstance().getSystemStatsJson() + "\n";
      sendJsonResponse(c, hm, 200, json);
      return;
    }

    // System Reboot
    if (mg_match(hm->uri, mg_str("/api/system/reboot"), NULL)) {
      sendJsonResponse(c, hm, 200, "{\"rebooting\":true}\n");
      mg_timer_add(&server->_mgr, 1000, 0, [](void*) {
        esp_restart();
      }, NULL);
      return;
    }

    // 404 Fallback
    mg_http_reply(c, 404,
                  "Content-Type: text/plain\r\n"
                  "Access-Control-Allow-Origin: *\r\n"
                  "Access-Control-Allow-Private-Network: true\r\n",
                  "404 Not Found\n");
  }
}
