#ifndef SMARTHOME_DEVICE_MANAGER_H
#define SMARTHOME_DEVICE_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <functional>
#include "config.h"
#include "auth_manager.h"

enum DeviceType {
  DEV_SWITCH = 0,  // Light or On/Off switch
  DEV_DIMMER = 1,  // Dimmer (0 - 100% PWM brightness)
  DEV_FAN = 2,     // Ceiling / Table Fan
  DEV_RELAY = 3    // Socket / Appliance Relay (Active Low/High)
};

struct Device {
  String id;
  String name;
  String roomId;
  DeviceType type;
  int pin;         // ESP32 GPIO pin or -1 for virtual
  bool inverted;   // true if active-LOW (common for relay modules)
  int pwmChannel;  // 0-15 assigned for dimmers
  bool state;
  int value;       // 0-100 for dimmer
  String allowedUsers; // "*" for everyone, or comma-separated usernames e.g. "admin,alice"
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

  // Control operations
  bool setDeviceState(const String& deviceId, bool state, int value = -1);
  bool toggleDevice(const String& deviceId);
  bool canUserAccessDevice(const User& user, const Device& dev);

  // Admin Dynamic CRUD
  bool addDevice(Device newDev, String& errorMsg);
  bool updateDevice(Device updatedDev, String& errorMsg);
  bool deleteDevice(const String& deviceId, String& errorMsg);

  bool addRoom(const Room& newRoom, String& errorMsg);
  bool deleteRoom(const String& roomId, String& errorMsg);

  Device* getDevice(const String& deviceId);
  SensorReadings getSensors() const { return _sensors; }

  // JSON representations
  String getDevicesJsonForUser(const User& user);
  String getAllDevicesJson();
  String getSensorsJson();
  String getSystemStatsJson();

  const std::vector<Room>& getRooms() const { return _rooms; }
  const std::vector<Device>& getDevices() const { return _devices; }

  static bool isSafePin(int pin);

private:
  DeviceManager();
  ~DeviceManager() = default;

  void loadFromNvs();
  void saveToNvs();
  void initDefaults();

  void configureHardwarePin(Device& dev);
  void deconfigureHardwarePin(const Device& dev);
  void updateHardwarePin(const Device& dev);
  int allocatePwmChannel();

  void updateSensors();

  std::vector<Room> _rooms;
  std::vector<Device> _devices;
  SensorReadings _sensors;

  unsigned long _lastSensorUpdate;
  unsigned long _uptimeStart;
  DeviceChangeCallback _changeCallback;
  int _nextPwmChannel;
};

#endif // SMARTHOME_DEVICE_MANAGER_H
