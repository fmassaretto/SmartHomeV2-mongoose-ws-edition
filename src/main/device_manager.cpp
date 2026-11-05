#include "device_manager.h"
#include <esp_system.h>
#include <esp_ota_ops.h>
#include "wifi_manager.h"

DeviceManager& DeviceManager::getInstance() {
  static DeviceManager instance;
  return instance;
}

DeviceManager::DeviceManager()
    : _lastSensorUpdate(0),
      _uptimeStart(millis()),
      _changeCallback(nullptr) {
  _sensors.temperature = 24.2f;
  _sensors.humidity = 52.0f;
  _sensors.motion = false;
  _sensors.lightLux = 320;
  _sensors.powerWatts = 145.0f;
}

void DeviceManager::initHardware() {
  // Define standard rooms
  _rooms = {
    {"living_room", "Living Room", "sofa"},
    {"bedroom_1",   "Master Bedroom", "bed"},
    {"bedroom_2",   "Kid's Bedroom", "sparkles"},
    {"kitchen",     "Kitchen", "utensils"},
    {"balcony",     "Balcony & Garden", "sun"}
  };

  // Define standard devices mapped to pins
  _devices = {
    {"light_living",  "Living Room Light",   "living_room", DEV_SWITCH, PIN_LIGHT_LIVING,  false, 0},
    {"dimmer_living", "Ceiling Chandelier",  "living_room", DEV_DIMMER, PIN_DIMMER_LIVING, false, 75},
    {"fan_living",    "Ceiling Fan",         "living_room", DEV_FAN,    PIN_FAN_LIVING,    false, 0},
    {"light_bed1",    "Master Bedroom Light","bedroom_1",   DEV_SWITCH, PIN_LIGHT_BED1,    false, 0},
    {"light_bed2",    "Kid's Bedroom Light", "bedroom_2",   DEV_SWITCH, PIN_LIGHT_BED2,    false, 0},
    {"light_kitchen", "Kitchen Downlight",   "kitchen",     DEV_SWITCH, PIN_LIGHT_KITCHEN, false, 0},
    {"light_balcony", "Balcony Accent",      "balcony",     DEV_SWITCH, PIN_LIGHT_BALCONY, false, 0}
  };

  // Configure GPIO pins
  for (const auto& dev : _devices) {
    if (dev.pin >= 0) {
      if (dev.type == DEV_DIMMER) {
        ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
        ledcAttachPin(dev.pin, PWM_CHANNEL);
        ledcWrite(PWM_CHANNEL, 0);
      } else {
        pinMode(dev.pin, OUTPUT);
        digitalWrite(dev.pin, LOW);
      }
    }
  }

  Serial.printf("[DEV] Initialized %d rooms and %d devices\n",
                (int)_rooms.size(), (int)_devices.size());
}

void DeviceManager::begin() {
  initHardware();
}

void DeviceManager::setChangeCallback(DeviceChangeCallback cb) {
  _changeCallback = cb;
}

void DeviceManager::updateHardwarePin(const Device& dev) {
  if (dev.pin < 0) return;

  if (dev.type == DEV_DIMMER) {
    uint32_t duty = dev.state ? (uint32_t)(dev.value * 255 / 100) : 0;
    ledcWrite(PWM_CHANNEL, duty);
  } else {
    digitalWrite(dev.pin, dev.state ? HIGH : LOW);
  }
}

bool DeviceManager::setDeviceState(const String& deviceId, bool state, int value) {
  for (auto& dev : _devices) {
    if (dev.id == deviceId) {
      dev.state = state;
      if (value >= 0 && value <= 100) {
        dev.value = value;
      }
      updateHardwarePin(dev);

      Serial.printf("[DEV] Device '%s' state changed -> %s (value: %d)\n",
                    dev.id.c_str(), dev.state ? "ON" : "OFF", dev.value);

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

  // Realistic dynamic sensor readings with gentle ambient drift
  static float phase = 0.0f;
  phase += 0.05f;

  _sensors.temperature = 23.5f + sin(phase) * 1.8f;
  _sensors.humidity = 50.0f + cos(phase * 0.7f) * 6.0f;

  // Occasional motion trigger (e.g. every ~30 seconds)
  static int motionCounter = 0;
  if (++motionCounter > 10) {
    _sensors.motion = !_sensors.motion;
    motionCounter = 0;
  }

  // Calculate current power based on active devices
  float power = 35.0f; // Baseline standby
  for (const auto& dev : _devices) {
    if (dev.state) {
      if (dev.type == DEV_DIMMER) {
        power += (dev.value / 100.0f) * 60.0f;
      } else if (dev.type == DEV_FAN) {
        power += 55.0f;
      } else {
        power += 15.0f; // 15W LED
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

String DeviceManager::getDevicesJsonForUser(const User& user) {
  AuthManager& auth = AuthManager::getInstance();

  String json = "{";
  // Rooms allowed for this user
  json += "\"rooms\":[";
  bool firstRoom = true;
  for (const auto& room : _rooms) {
    if (auth.canAccessRoom(user, room.id)) {
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

  // Devices allowed for this user
  json += "\"devices\":[";
  bool firstDev = true;
  for (const auto& dev : _devices) {
    if (auth.canAccessDevice(user, dev.roomId, dev.id)) {
      if (!firstDev) json += ",";
      firstDev = false;
      json += "{";
      json += "\"id\":\"" + dev.id + "\",";
      json += "\"name\":\"" + dev.name + "\",";
      json += "\"roomId\":\"" + dev.roomId + "\",";
      json += "\"type\":" + String((int)dev.type) + ",";
      json += "\"pin\":" + String(dev.pin) + ",";
      json += "\"state\":" + String(dev.state ? "true" : "false") + ",";
      json += "\"value\":" + String(dev.value);
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
    json += "\"state\":" + String(dev.state ? "true" : "false") + ",";
    json += "\"value\":" + String(dev.value);
    json += "}";
  }
  json += "],";

  json += "\"sensors\":" + getSensorsJson();
  json += "}";
  return json;
}

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
