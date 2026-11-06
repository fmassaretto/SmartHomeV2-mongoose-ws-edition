#include "device_manager.h"
#include <esp_system.h>
#include <esp_ota_ops.h>
#include <Preferences.h>
#include "wifi_manager.h"

static Preferences s_devPrefs;

DeviceManager& DeviceManager::getInstance() {
  static DeviceManager instance;
  return instance;
}

DeviceManager::DeviceManager()
    : _lastSensorUpdate(0),
      _uptimeStart(millis()),
      _changeCallback(nullptr),
      _nextPwmChannel(0) {
  _sensors.temperature = 24.2f;
  _sensors.humidity = 52.0f;
  _sensors.motion = false;
  _sensors.lightLux = 320;
  _sensors.powerWatts = 145.0f;
}

bool DeviceManager::isSafePin(int pin) {
  if (pin < 0) return true; // -1 is virtual pin (always allowed)
  // Standard safe output GPIOs on ESP32
  static const int safePins[] = {2, 4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33};
  for (int p : safePins) {
    if (p == pin) return true;
  }
  return false;
}

int DeviceManager::allocatePwmChannel() {
  int ch = _nextPwmChannel;
  _nextPwmChannel = (_nextPwmChannel + 1) % 16;
  return ch;
}

void DeviceManager::initDefaults() {
  _rooms = {
    {"living_room", "Living Room", "sofa"},
    {"bedroom_1",   "Master Bedroom", "bed"},
    {"bedroom_2",   "Kid's Bedroom", "sparkles"},
    {"kitchen",     "Kitchen", "utensils"},
    {"balcony",     "Balcony & Garden", "sun"}
  };

  _devices = {
    {"light_living",  "Living Room Light",   "living_room", DEV_SWITCH, PIN_LIGHT_LIVING,  false, 0, false, 0, "*"},
    {"dimmer_living", "Ceiling Chandelier",  "living_room", DEV_DIMMER, PIN_DIMMER_LIVING, false, 0, false, 75, "*"},
    {"fan_living",    "Ceiling Fan",         "living_room", DEV_FAN,    PIN_FAN_LIVING,    false, 0, false, 0, "*"},
    {"light_bed1",    "Master Bedroom Light","bedroom_1",   DEV_SWITCH, PIN_LIGHT_BED1,    false, 0, false, 0, "*"},
    {"light_bed2",    "Kid's Bedroom Light", "bedroom_2",   DEV_SWITCH, PIN_LIGHT_BED2,    false, 0, false, 0, "*"},
    {"light_kitchen", "Kitchen Downlight",   "kitchen",     DEV_SWITCH, PIN_LIGHT_KITCHEN, false, 0, false, 0, "*"},
    {"light_balcony", "Balcony Accent",      "balcony",     DEV_SWITCH, PIN_LIGHT_BALCONY, false, 0, false, 0, "*"}
  };

  saveToNvs();
  Serial.printf("[DEV] Initialized and saved %d default rooms and %d devices\n",
                (int)_rooms.size(), (int)_devices.size());
}

void DeviceManager::loadFromNvs() {
  s_devPrefs.begin("smarthome_dev", false);
  int roomCount = s_devPrefs.getInt("r_count", 0);
  int devCount = s_devPrefs.getInt("d_count", 0);

  if (roomCount <= 0 || devCount <= 0) {
    s_devPrefs.end();
    initDefaults();
    return;
  }

  // Load Rooms
  _rooms.clear();
  for (int i = 0; i < roomCount; i++) {
    String key = "r_" + String(i);
    String raw = s_devPrefs.getString(key.c_str(), "");
    if (raw.length() == 0) continue;

    int p1 = raw.indexOf('|');
    int p2 = raw.indexOf('|', p1 + 1);
    if (p1 > 0 && p2 > 0) {
      Room r;
      r.id = raw.substring(0, p1);
      r.name = raw.substring(p1 + 1, p2);
      r.icon = raw.substring(p2 + 1);
      _rooms.push_back(r);
    }
  }

  // Load Devices
  _devices.clear();
  for (int i = 0; i < devCount; i++) {
    String key = "d_" + String(i);
    String raw = s_devPrefs.getString(key.c_str(), "");
    if (raw.length() == 0) continue;

    // Format: id|name|roomId|type|pin|inverted|state|value|pwmChannel[|allowedUsers]
    int idx[9];
    int start = 0;
    int found = 0;
    for (int k = 0; k < 9; k++) {
      int p = raw.indexOf('|', start);
      if (p == -1) break;
      idx[k] = p;
      start = p + 1;
      found++;
    }

    if (found >= 8) {
      Device d;
      d.id = raw.substring(0, idx[0]);
      d.name = raw.substring(idx[0] + 1, idx[1]);
      d.roomId = raw.substring(idx[1] + 1, idx[2]);
      d.type = (DeviceType)raw.substring(idx[2] + 1, idx[3]).toInt();
      d.pin = raw.substring(idx[3] + 1, idx[4]).toInt();
      d.inverted = raw.substring(idx[4] + 1, idx[5]).toInt() == 1;
      d.state = raw.substring(idx[5] + 1, idx[6]).toInt() == 1;
      d.value = raw.substring(idx[6] + 1, idx[7]).toInt();
      if (found >= 9) {
        d.pwmChannel = raw.substring(idx[7] + 1, idx[8]).toInt();
        d.allowedUsers = raw.substring(idx[8] + 1);
      } else {
        d.pwmChannel = raw.substring(idx[7] + 1).toInt();
        d.allowedUsers = "*";
      }
      if (d.allowedUsers.length() == 0) d.allowedUsers = "*";
      _devices.push_back(d);
    }
  }
  s_devPrefs.end();

  if (_devices.empty() || _rooms.empty()) {
    initDefaults();
  } else {
    Serial.printf("[DEV] Loaded %d rooms and %d devices from NVS\n",
                  (int)_rooms.size(), (int)_devices.size());
  }
}

void DeviceManager::saveToNvs() {
  s_devPrefs.begin("smarthome_dev", false);
  s_devPrefs.clear();

  // Save Rooms
  s_devPrefs.putInt("r_count", (int)_rooms.size());
  for (size_t i = 0; i < _rooms.size(); i++) {
    const Room& r = _rooms[i];
    String raw = r.id + "|" + r.name + "|" + r.icon;
    String key = "r_" + String(i);
    s_devPrefs.putString(key.c_str(), raw);
  }

  // Save Devices
  s_devPrefs.putInt("d_count", (int)_devices.size());
  for (size_t i = 0; i < _devices.size(); i++) {
    const Device& d = _devices[i];
    String raw = d.id + "|" + d.name + "|" + d.roomId + "|" + String((int)d.type) + "|"
               + String(d.pin) + "|" + String(d.inverted ? 1 : 0) + "|"
               + String(d.state ? 1 : 0) + "|" + String(d.value) + "|" + String(d.pwmChannel) + "|"
               + (d.allowedUsers.length() > 0 ? d.allowedUsers : "*");
    String key = "d_" + String(i);
    s_devPrefs.putString(key.c_str(), raw);
  }
  s_devPrefs.end();
}

void DeviceManager::configureHardwarePin(Device& dev) {
  if (dev.pin < 0) return;

  if (dev.type == DEV_DIMMER) {
    if (dev.pwmChannel < 0 || dev.pwmChannel > 15) {
      dev.pwmChannel = allocatePwmChannel();
    }
    ledcSetup(dev.pwmChannel, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(dev.pin, dev.pwmChannel);
    updateHardwarePin(dev);
    Serial.printf("[DEV-PIN] Configured Dimmer Pin %d on PWM Channel %d\n", dev.pin, dev.pwmChannel);
  } else {
    pinMode(dev.pin, OUTPUT);
    updateHardwarePin(dev);
    Serial.printf("[DEV-PIN] Configured Output Pin %d (inverted: %s)\n",
                  dev.pin, dev.inverted ? "yes" : "no");
  }
}

void DeviceManager::deconfigureHardwarePin(const Device& dev) {
  if (dev.pin < 0) return;

  if (dev.type == DEV_DIMMER) {
    ledcDetachPin(dev.pin);
  }
  digitalWrite(dev.pin, LOW);
  pinMode(dev.pin, INPUT);
}

void DeviceManager::updateHardwarePin(const Device& dev) {
  if (dev.pin < 0) return;

  if (dev.type == DEV_DIMMER) {
    uint32_t duty = 0;
    if (dev.state) {
      duty = (uint32_t)(dev.value * 255 / 100);
      if (dev.inverted) duty = 255 - duty;
    } else {
      duty = dev.inverted ? 255 : 0;
    }
    ledcWrite(dev.pwmChannel, duty);
  } else {
    int level = dev.state ? HIGH : LOW;
    if (dev.inverted) level = (level == HIGH) ? LOW : HIGH;
    digitalWrite(dev.pin, level);
  }
}

void DeviceManager::begin() {
  loadFromNvs();

  // Configure physical pins for all loaded devices
  for (auto& dev : _devices) {
    configureHardwarePin(dev);
  }
}

void DeviceManager::setChangeCallback(DeviceChangeCallback cb) {
  _changeCallback = cb;
}

bool DeviceManager::setDeviceState(const String& deviceId, bool state, int value) {
  for (auto& dev : _devices) {
    if (dev.id == deviceId) {
      dev.state = state;
      if (value >= 0 && value <= 100) {
        dev.value = value;
      }
      updateHardwarePin(dev);

      Serial.printf("[DEV] Device '%s' state changed -> %s (value: %d, pin: %d)\n",
                    dev.id.c_str(), dev.state ? "ON" : "OFF", dev.value, dev.pin);

      if (_changeCallback) {
        String payload = "{";
        payload += "\"deviceId\":\"" + dev.id + "\",";
        payload += "\"state\":" + String(dev.state ? "true" : "false") + ",";
        payload += "\"value\":" + String(dev.value);
        payload += "}";
        _changeCallback("device_update", payload);
      }
      return true;
    }
  }
  return false;
}

bool DeviceManager::toggleDevice(const String& deviceId) {
  for (const auto& dev : _devices) {
    if (dev.id == deviceId) {
      return setDeviceState(deviceId, !dev.state, dev.value);
    }
  }
  return false;
}

bool DeviceManager::addDevice(Device newDev, String& errorMsg) {
  if (newDev.name.length() < 2) {
    errorMsg = "Name must be at least 2 characters";
    return false;
  }

  // Generate ID if empty
  if (newDev.id.length() == 0) {
    newDev.id = "dev_" + String(millis());
  }

  for (const auto& d : _devices) {
    if (d.id == newDev.id) {
      errorMsg = "Device ID already exists";
      return false;
    }
    if (newDev.pin >= 0 && d.pin == newDev.pin) {
      errorMsg = "GPIO pin " + String(newDev.pin) + " is already in use by '" + d.name + "'";
      return false;
    }
  }

  if (!isSafePin(newDev.pin)) {
    errorMsg = "GPIO pin " + String(newDev.pin) + " is reserved or unsafe on ESP32";
    return false;
  }

  if (newDev.type == DEV_DIMMER) {
    newDev.pwmChannel = allocatePwmChannel();
    if (newDev.value <= 0) newDev.value = 75;
  }

  if (newDev.allowedUsers.length() == 0) {
    newDev.allowedUsers = "*";
  }

  configureHardwarePin(newDev);
  _devices.push_back(newDev);
  saveToNvs();

  Serial.printf("[DEV-ADMIN] Added new control '%s' on pin %d (room: %s)\n",
                newDev.name.c_str(), newDev.pin, newDev.roomId.c_str());

  if (_changeCallback) {
    _changeCallback("device_list_changed", getAllDevicesJson());
  }
  return true;
}

bool DeviceManager::updateDevice(Device updatedDev, String& errorMsg) {
  for (auto& dev : _devices) {
    if (dev.id == updatedDev.id) {
      // Check if pin changed and collides with another device
      if (updatedDev.pin >= 0 && updatedDev.pin != dev.pin) {
        if (!isSafePin(updatedDev.pin)) {
          errorMsg = "GPIO pin " + String(updatedDev.pin) + " is unsafe on ESP32";
          return false;
        }
        for (const auto& other : _devices) {
          if (other.id != dev.id && other.pin == updatedDev.pin) {
            errorMsg = "GPIO pin " + String(updatedDev.pin) + " is already in use by '" + other.name + "'";
            return false;
          }
        }
        deconfigureHardwarePin(dev);
        dev.pin = updatedDev.pin;
      }

      dev.name = updatedDev.name;
      dev.roomId = updatedDev.roomId;
      dev.type = updatedDev.type;
      dev.inverted = updatedDev.inverted;
      if (updatedDev.allowedUsers.length() > 0) {
        dev.allowedUsers = updatedDev.allowedUsers;
      }
      configureHardwarePin(dev);

      saveToNvs();
      Serial.printf("[DEV-ADMIN] Updated control '%s' on pin %d\n", dev.name.c_str(), dev.pin);

      if (_changeCallback) {
        _changeCallback("device_list_changed", getAllDevicesJson());
      }
      return true;
    }
  }
  errorMsg = "Device not found";
  return false;
}

bool DeviceManager::deleteDevice(const String& deviceId, String& errorMsg) {
  for (auto it = _devices.begin(); it != _devices.end(); ++it) {
    if (it->id == deviceId) {
      deconfigureHardwarePin(*it);
      Serial.printf("[DEV-ADMIN] Deleted control '%s' (pin: %d)\n", it->name.c_str(), it->pin);
      _devices.erase(it);
      saveToNvs();

      if (_changeCallback) {
        _changeCallback("device_list_changed", getAllDevicesJson());
      }
      return true;
    }
  }
  errorMsg = "Device not found";
  return false;
}

bool DeviceManager::addRoom(const Room& newRoom, String& errorMsg) {
  if (newRoom.id.length() < 2 || newRoom.name.length() < 2) {
    errorMsg = "Room ID and name must be at least 2 chars";
    return false;
  }

  for (const auto& r : _rooms) {
    if (r.id == newRoom.id) {
      errorMsg = "Room ID already exists";
      return false;
    }
  }

  _rooms.push_back(newRoom);
  saveToNvs();
  Serial.printf("[DEV-ADMIN] Added new room '%s' (%s)\n", newRoom.name.c_str(), newRoom.id.c_str());

  if (_changeCallback) {
    _changeCallback("device_list_changed", getAllDevicesJson());
  }
  return true;
}

bool DeviceManager::deleteRoom(const String& roomId, String& errorMsg) {
  for (const auto& dev : _devices) {
    if (dev.roomId == roomId) {
      errorMsg = "Cannot delete room: devices are still assigned to it";
      return false;
    }
  }

  for (auto it = _rooms.begin(); it != _rooms.end(); ++it) {
    if (it->id == roomId) {
      _rooms.erase(it);
      saveToNvs();
      if (_changeCallback) {
        _changeCallback("device_list_changed", getAllDevicesJson());
      }
      return true;
    }
  }
  errorMsg = "Room not found";
  return false;
}

Device* DeviceManager::getDevice(const String& deviceId) {
  for (auto& dev : _devices) {
    if (dev.id == deviceId) {
      return &dev;
    }
  }
  return nullptr;
}

void DeviceManager::updateSensors() {
  unsigned long now = millis();
  if (now - _lastSensorUpdate < 3000) return;
  _lastSensorUpdate = now;

  static float phase = 0.0f;
  phase += 0.05f;

  _sensors.temperature = 23.5f + sin(phase) * 1.8f;
  _sensors.humidity = 50.0f + cos(phase * 0.7f) * 6.0f;

  static int motionCounter = 0;
  if (++motionCounter > 10) {
    _sensors.motion = !_sensors.motion;
    motionCounter = 0;
  }

  float power = 35.0f;
  for (const auto& dev : _devices) {
    if (dev.state) {
      if (dev.type == DEV_DIMMER) {
        power += (dev.value / 100.0f) * 60.0f;
      } else if (dev.type == DEV_FAN) {
        power += 55.0f;
      } else {
        power += 15.0f;
      }
    }
  }
  _sensors.powerWatts = power;
  _sensors.lightLux = _devices[0].state ? 450 : 180;

  if (_changeCallback) {
    _changeCallback("sensors_update", getSensorsJson());
  }
}

void DeviceManager::loop() {
  updateSensors();
}

String DeviceManager::getSensorsJson() {
  String json = "{";
  json += "\"temperature\":" + String(_sensors.temperature, 1) + ",";
  json += "\"humidity\":" + String(_sensors.humidity, 1) + ",";
  json += "\"motion\":" + String(_sensors.motion ? "true" : "false") + ",";
  json += "\"lightLux\":" + String(_sensors.lightLux) + ",";
  json += "\"powerWatts\":" + String(_sensors.powerWatts, 1);
  json += "}";
  return json;
}

bool DeviceManager::canUserAccessDevice(const User& user, const Device& dev) {
  if (user.isAdmin()) return true;

  // If set to allow everyone / wildcard
  if (dev.allowedUsers == "*" || dev.allowedUsers.equalsIgnoreCase("everyone") ||
      dev.allowedUsers.equalsIgnoreCase("all") || dev.allowedUsers.length() == 0) {
    return true;
  }

  // Check specific usernames in comma-separated list
  int start = 0;
  while (start < (int)dev.allowedUsers.length()) {
    int comma = dev.allowedUsers.indexOf(',', start);
    String u = (comma == -1) ? dev.allowedUsers.substring(start) : dev.allowedUsers.substring(start, comma);
    u.trim();
    if (u.equalsIgnoreCase(user.username)) {
      return true;
    }
    if (comma == -1) break;
    start = comma + 1;
  }

  return false;
}

String DeviceManager::getDevicesJsonForUser(const User& user) {
  AuthManager& auth = AuthManager::getInstance();

  String json = "{";
  json += "\"rooms\":[";
  bool firstRoom = true;
  for (const auto& room : _rooms) {
    // Show room if user has room permission OR has at least one accessible device in this room
    bool hasAccessibleDevice = false;
    for (const auto& dev : _devices) {
      if (dev.roomId == room.id && canUserAccessDevice(user, dev)) {
        hasAccessibleDevice = true;
        break;
      }
    }

    if (user.isAdmin() || auth.canAccessRoom(user, room.id) || hasAccessibleDevice) {
      if (!firstRoom) json += ",";
      firstRoom = false;
      json += "{";
      json += "\"id\":\"" + room.id + "\",";
      json += "\"name\":\"" + room.name + "\",";
      json += "\"icon\":\"" + room.icon + "\"";
      json += "}";
    }
  }
  json += "],";

  json += "\"devices\":[";
  bool firstDev = true;
  for (const auto& dev : _devices) {
    if (canUserAccessDevice(user, dev)) {
      if (!firstDev) json += ",";
      firstDev = false;
      json += "{";
      json += "\"id\":\"" + dev.id + "\",";
      json += "\"name\":\"" + dev.name + "\",";
      json += "\"roomId\":\"" + dev.roomId + "\",";
      json += "\"type\":" + String((int)dev.type) + ",";
      json += "\"pin\":" + String(dev.pin) + ",";
      json += "\"inverted\":" + String(dev.inverted ? "true" : "false") + ",";
      json += "\"state\":" + String(dev.state ? "true" : "false") + ",";
      json += "\"value\":" + String(dev.value) + ",";
      json += "\"allowedUsers\":\"" + (dev.allowedUsers.length() > 0 ? dev.allowedUsers : "*") + "\"";
      json += "}";
    }
  }
  json += "],";

  json += "\"sensors\":" + getSensorsJson();
  json += "}";
  return json;
}

String DeviceManager::getAllDevicesJson() {
  String json = "{";
  json += "\"rooms\":[";
  for (size_t i = 0; i < _rooms.size(); i++) {
    if (i > 0) json += ",";
    json += "{\"id\":\"" + _rooms[i].id + "\",\"name\":\"" + _rooms[i].name + "\",\"icon\":\"" + _rooms[i].icon + "\"}";
  }
  json += "],";

  json += "\"devices\":[";
  for (size_t i = 0; i < _devices.size(); i++) {
    if (i > 0) json += ",";
    const auto& dev = _devices[i];
    json += "{";
    json += "\"id\":\"" + dev.id + "\",";
    json += "\"name\":\"" + dev.name + "\",";
    json += "\"roomId\":\"" + dev.roomId + "\",";
    json += "\"type\":" + String((int)dev.type) + ",";
    json += "\"pin\":" + String(dev.pin) + ",";
    json += "\"inverted\":" + String(dev.inverted ? "true" : "false") + ",";
    json += "\"state\":" + String(dev.state ? "true" : "false") + ",";
    json += "\"value\":" + String(dev.value) + ",";
    json += "\"allowedUsers\":\"" + (dev.allowedUsers.length() > 0 ? dev.allowedUsers : "*") + "\"";
    json += "}";
  }
  json += "],";

  json += "\"sensors\":" + getSensorsJson();
  json += "}";
  return json;
}
//   json += "],";

//   json += "\"sensors\":" + getSensorsJson();
//   json += "}";
//   return json;
// }

String DeviceManager::getSystemStatsJson() {
  uint32_t freeHeap = esp_get_free_heap_size();
  uint32_t minHeap = esp_get_minimum_free_heap_size();
  unsigned long uptimeSec = (millis() - _uptimeStart) / 1000;

  const esp_partition_t* running = esp_ota_get_running_partition();
  String otaLabel = running ? String(running->label) : "app0";

  String json = "{";
  json += "\"version\":\"" + String(FIRMWARE_VERSION) + "\",";
  json += "\"deviceName\":\"" + String(DEVICE_NAME) + "\",";
  json += "\"uptime\":" + String(uptimeSec) + ",";
  json += "\"freeHeap\":" + String(freeHeap) + ",";
  json += "\"minHeap\":" + String(minHeap) + ",";
  json += "\"cpuFreq\":" + String(getCpuFrequencyMhz()) + ",";
  json += "\"otaPartition\":\"" + otaLabel + "\",";
  json += "\"wifi\":" + WifiManager::getInstance().getStatusJson();
  json += "}";
  return json;
}
