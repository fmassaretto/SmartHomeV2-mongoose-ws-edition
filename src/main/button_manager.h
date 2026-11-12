#ifndef SMARTHOME_BUTTON_MANAGER_H
#define SMARTHOME_BUTTON_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "device_manager.h"

enum ButtonMode {
  BTN_MODE_PUSH = 0,    // Momentary Push Button: Toggle device on press (tactile/bell button)
  BTN_MODE_ROCKER = 1,  // Wall Rocker Switch: Toggle device on any edge flip (standard household switch)
  BTN_MODE_FOLLOW = 2   // Follow Level: Active level = ON, Inactive level = OFF
};

enum PullMode {
  PULL_UP = 0,    // Internal Pull-Up (Default): Button connects pin to GND
  PULL_DOWN = 1,  // Internal Pull-Down: Button connects pin to 3.3V
  PULL_NONE = 2   // No Internal Pull: External resistor (required for input-only GPIO 34-39)
};

enum ButtonAction {
  TRIGGER_TOGGLE = 0,
  TRIGGER_SET_ON = 1,
  TRIGGER_SET_OFF = 2
};

struct PhysicalButton {
  String id;             // Unique ID, e.g. "btn_1", "btn_169999999"
  String name;           // Friendly name, e.g. "Living Room Wall Switch"
  int pin;               // ESP32 GPIO pin (e.g. 13, 14, 27, 34...)
  ButtonMode mode;       // BTN_MODE_PUSH, BTN_MODE_ROCKER, BTN_MODE_FOLLOW
  PullMode pull;         // PULL_UP, PULL_DOWN, PULL_NONE
  String targetDevices;  // Comma-separated device IDs e.g. "light_living" or "light_living,fan_living" or "*"
  bool enabled;          // true if active

  // Runtime state (not saved to NVS)
  int lastRawState;
  int debouncedState;
  unsigned long lastDebounceTime;
};

class ButtonManager {
public:
  static ButtonManager& getInstance();

  void begin();
  void loop();

  // CRUD Operations
  bool addButton(PhysicalButton btn, String& errorMsg);
  bool updateButton(PhysicalButton btn, String& errorMsg);
  bool deleteButton(const String& btnId, String& errorMsg);
  bool triggerButton(const String& btnId, String& errorMsg);

  PhysicalButton* getButton(const String& btnId);
  const std::vector<PhysicalButton>& getButtons() const { return _buttons; }

  // Check pin usage
  bool isPinUsedByButton(int pin, const String& excludeBtnId = "");

  // Safe input pin validation
  static bool isSafeInputPin(int pin);

  // JSON representations
  String getButtonsJson();

private:
  ButtonManager();
  ~ButtonManager() = default;

  void loadFromNvs();
  void saveToNvs();

  void configureButtonPin(PhysicalButton& btn);
  void deconfigureButtonPin(int pin);
  void executeAction(const PhysicalButton& btn, ButtonAction action);

  std::vector<PhysicalButton> _buttons;
  bool _initialized;
};

#endif // SMARTHOME_BUTTON_MANAGER_H
