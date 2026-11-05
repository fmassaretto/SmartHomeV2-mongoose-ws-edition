#ifndef SMARTHOME_WEB_SERVER_H
#define SMARTHOME_WEB_SERVER_H

#include <Arduino.h>
#include "../mongoose/mongoose.h"
#include "config.h"

class WebServer {
public:
  static WebServer& getInstance();

  void begin();
  void poll();

  // Push WebSocket notification to all connected clients
  void broadcastWs(const String& eventType, const String& payloadJson);

private:
  WebServer();
  ~WebServer() = default;

  static void httpEventHandler(struct mg_connection *c, int ev, void *ev_data);

  struct mg_mgr _mgr;
  bool _initialized;
};

#endif // SMARTHOME_WEB_SERVER_H
