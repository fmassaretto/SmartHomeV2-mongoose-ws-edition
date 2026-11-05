#ifndef SMARTHOME_DEVICE_MANAGER_H
#define SMARTHOME_DEVICE_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <functional>
#include "config.h"
#include "auth_manager.h"

enum DeviceType {
  DEV_SWITCH,  // On / Off light or relay
  DEV_DIMMER,  // 0 - 100% brightness
  DEV_FAN      // On / Off fan
};

struct Device {
  String id;
  String name;
  String roomId;
  DeviceType type;
  int pin;
  bool state;
  int value; // 0-100 for dimmer
};

struct Room {
  String id;
  String name;
  String icon;
};

struct SensorReadings {
  float temperature;
  float humidity;
  bool motion;
  int lightLux;
  float powerWatts;
};

typedef std::function<void(const String& eventType, const String& payloadJson)> DeviceChangeCallback;

class DeviceManager {
public:
  static DeviceManager& getInstance();

  void begin();
  void loop();

  void setChangeCallback(DeviceChangeCallback cb);

  bool setDeviceState(const String& deviceId, bool state, int value = -1);
  bool toggleDevice(const String& deviceId);

  Device* getDevice(const String& deviceId);
  SensorReadings getSensors() const { return _sensors; }

  // JSON representations
  String getDevicesJsonForUser(const User& user);
  String getAllDevicesJson();
  String getSensorsJson();
  String getSystemStatsJson();

  const std::vector<Room>& getRooms() const { return _rooms; }
  const std::vector<Device>& getDevices() const { return _devices; }

private:
  DeviceManager();
  ~DeviceManager() = default;

  void initHardware();
  void updateHardwarePin(const Device& dev);
  void updateSensors();

  std::vector<Room> _rooms;
  std::vector<Device> _devices;
  SensorReadings _sensors;

  unsigned long _lastSensorUpdate;
  unsigned long _uptimeStart;
  DeviceChangeCallback _changeCallback;
};

#endif // SMARTHOME_DEVICE_MANAGER_H
