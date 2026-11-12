#include "button_manager.h"
#include <Preferences.h>

static Preferences s_btnPrefs;

ButtonManager& ButtonManager::getInstance() {
  static ButtonManager instance;
  return instance;
}

ButtonManager::ButtonManager() : _initialized(false) {}

bool ButtonManager::isSafeInputPin(int pin) {
  if (pin < 0) return true; // -1 = unassigned / virtual
  // ESP32 input-capable GPIOs (excluding UART0 GPIO 1/3 and SPI Flash GPIO 6-11)
  static const int safeInputPins[] = {
    0, 2, 4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33, 34, 35, 36, 39
  };
  for (int p : safeInputPins) {
    if (p == pin) return true;
  }
  return false;
}

bool ButtonManager::isPinUsedByButton(int pin, const String& excludeBtnId) {
  if (pin < 0) return false;
  for (const auto& b : _buttons) {
    if (b.id != excludeBtnId && b.pin == pin) return true;
  }
  return false;
}

void ButtonManager::configureButtonPin(PhysicalButton& btn) {
  if (btn.pin < 0) return;

  // GPIO 34, 35, 36 (VP), 39 (VN) are input-only and lack internal pull resistors
  if (btn.pin == 34 || btn.pin == 35 || btn.pin == 36 || btn.pin == 39) {
    pinMode(btn.pin, INPUT);
    btn.pull = PULL_NONE;
  } else {
    if (btn.pull == PULL_UP) {
      pinMode(btn.pin, INPUT_PULLUP);
    } else if (btn.pull == PULL_DOWN) {
      pinMode(btn.pin, INPUT_PULLDOWN);
    } else {
      pinMode(btn.pin, INPUT);
    }
  }

  int initialReading = digitalRead(btn.pin);
  btn.lastRawState = initialReading;
  btn.debouncedState = initialReading;
  btn.lastDebounceTime = millis();

  Serial.printf("[BUTTON-PIN] Configured Input Pin GPIO %d for '%s' (mode: %d, pull: %d, initial: %d)\n",
                btn.pin, btn.name.c_str(), (int)btn.mode, (int)btn.pull, initialReading);
}

void ButtonManager::deconfigureButtonPin(int pin) {
  if (pin < 0) return;
  pinMode(pin, INPUT);
}

void ButtonManager::begin() {
  if (_initialized) return;
  loadFromNvs();

  for (auto& btn : _buttons) {
    if (btn.enabled) {
      configureButtonPin(btn);
    }
  }

  _initialized = true;
  Serial.printf("[BUTTON] Initialized Physical Button Manager (%d buttons loaded)\n", (int)_buttons.size());
}

void ButtonManager::loadFromNvs() {
  s_btnPrefs.begin("smarthome_btns", false);
  int count = s_btnPrefs.getInt("b_count", 0);
  _buttons.clear();

  for (int i = 0; i < count; i++) {
    String key = "b_" + String(i);
    String raw = s_btnPrefs.getString(key.c_str(), "");
    if (raw.length() == 0) continue;

    // Format: id|name|pin|mode|pull|targetDevices|enabled
    int idx[7];
    int start = 0;
    int found = 0;
    for (int k = 0; k < 7; k++) {
      int p = raw.indexOf('|', start);
      if (p == -1) break;
      idx[k] = p;
      start = p + 1;
      found++;
    }

    if (found >= 6) {
      PhysicalButton btn;
      btn.id = raw.substring(0, idx[0]);
      btn.name = raw.substring(idx[0] + 1, idx[1]);
      btn.pin = raw.substring(idx[1] + 1, idx[2]).toInt();
      btn.mode = (ButtonMode)raw.substring(idx[2] + 1, idx[3]).toInt();
      btn.pull = (PullMode)raw.substring(idx[3] + 1, idx[4]).toInt();
      btn.targetDevices = raw.substring(idx[4] + 1, idx[5]);
      btn.enabled = raw.substring(idx[5] + 1).toInt() == 1;

      btn.lastRawState = HIGH;
      btn.debouncedState = HIGH;
      btn.lastDebounceTime = 0;

      _buttons.push_back(btn);
    }
  }
  s_btnPrefs.end();
}

void ButtonManager::saveToNvs() {
  s_btnPrefs.begin("smarthome_btns", false);
  s_btnPrefs.clear();

  s_btnPrefs.putInt("b_count", (int)_buttons.size());
  for (size_t i = 0; i < _buttons.size(); i++) {
    const PhysicalButton& b = _buttons[i];
    String raw = b.id + "|" + b.name + "|" + String(b.pin) + "|"
               + String((int)b.mode) + "|" + String((int)b.pull) + "|"
               + b.targetDevices + "|" + String(b.enabled ? 1 : 0);
    String key = "b_" + String(i);
    s_btnPrefs.putString(key.c_str(), raw);
  }
  s_btnPrefs.end();
}

static std::vector<String> splitDeviceIds(const String& str) {
  std::vector<String> list;
  int start = 0;
  while (start < (int)str.length()) {
    int comma = str.indexOf(',', start);
    String item = (comma == -1) ? str.substring(start) : str.substring(start, comma);
    item.trim();
    if (item.length() > 0) list.push_back(item);
    if (comma == -1) break;
    start = comma + 1;
  }
  return list;
}

void ButtonManager::executeAction(const PhysicalButton& btn, ButtonAction action) {
  Serial.printf("[BUTTON] Action triggered from '%s' (GPIO %d) -> mode %d, action %d\n",
                btn.name.c_str(), btn.pin, (int)btn.mode, (int)action);

  std::vector<String> targets = splitDeviceIds(btn.targetDevices);
  auto& devMgr = DeviceManager::getInstance();

  if (targets.empty() || btn.targetDevices == "*") {
    // Control all devices
    const auto& all = devMgr.getDevices();
    for (const auto& d : all) {
      if (action == TRIGGER_TOGGLE) {
        devMgr.toggleDevice(d.id);
      } else if (action == TRIGGER_SET_ON) {
        devMgr.setDeviceState(d.id, true);
      } else if (action == TRIGGER_SET_OFF) {
        devMgr.setDeviceState(d.id, false);
      }
    }
    return;
  }

  for (const String& devId : targets) {
    if (action == TRIGGER_TOGGLE) {
      devMgr.toggleDevice(devId);
    } else if (action == TRIGGER_SET_ON) {
      devMgr.setDeviceState(devId, true);
    } else if (action == TRIGGER_SET_OFF) {
      devMgr.setDeviceState(devId, false);
    }
  }
}

void ButtonManager::loop() {
  if (_buttons.empty()) return;

  unsigned long now = millis();

  for (auto& btn : _buttons) {
    if (!btn.enabled || btn.pin < 0) continue;

    int reading = digitalRead(btn.pin);

    // Filter noise: if raw state changed, reset debounce timer
    if (reading != btn.lastRawState) {
      btn.lastDebounceTime = now;
      btn.lastRawState = reading;
    }

    // After state is stable for 50ms, process edge
    if ((now - btn.lastDebounceTime) > 50) {
      if (reading != btn.debouncedState) {
        int previousState = btn.debouncedState;
        btn.debouncedState = reading;

        // Determine active level (pressed/closed):
        // For PULL_UP: LOW is active (button pressed connects pin to GND)
        // For PULL_DOWN: HIGH is active (button pressed connects pin to 3.3V)
        // For PULL_NONE: default to LOW active unless external pull-down
        bool isActive = (btn.pull == PULL_DOWN) ? (reading == HIGH) : (reading == LOW);
        bool wasActive = (btn.pull == PULL_DOWN) ? (previousState == HIGH) : (previousState == LOW);

        if (btn.mode == BTN_MODE_PUSH) {
          // Momentary push button: trigger toggle on press edge only
          if (isActive && !wasActive) {
            executeAction(btn, TRIGGER_TOGGLE);
          }
        } else if (btn.mode == BTN_MODE_ROCKER) {
          // Wall rocker / toggle switch: toggle device on ANY position flip (rising or falling edge)
          executeAction(btn, TRIGGER_TOGGLE);
        } else if (btn.mode == BTN_MODE_FOLLOW) {
          // Follow level: ON when switch is active, OFF when inactive
          executeAction(btn, isActive ? TRIGGER_SET_ON : TRIGGER_SET_OFF);
        }
      }
    }
  }
}

bool ButtonManager::addButton(PhysicalButton btn, String& errorMsg) {
  if (btn.name.length() < 2) {
    errorMsg = "Button name must be at least 2 characters";
    return false;
  }

  if (btn.id.length() == 0) {
    btn.id = "btn_" + String(millis());
  }

  if (!isSafeInputPin(btn.pin)) {
    errorMsg = "GPIO " + String(btn.pin) + " is not a valid or safe input pin";
    return false;
  }

  if (btn.pin >= 0) {
    // Check if pin is used as an output by a device
    if (DeviceManager::getInstance().isPinUsedByDevice(btn.pin)) {
      Device* dev = DeviceManager::getInstance().getDeviceByPin(btn.pin);
      String dName = dev ? dev->name : "device";
      errorMsg = "GPIO " + String(btn.pin) + " is already assigned to device '" + dName + "'";
      return false;
    }
    // Check if pin is already used by another button
    if (isPinUsedByButton(btn.pin)) {
      PhysicalButton* otherBtn = getButtonByPin(btn.pin);
      String bName = otherBtn ? otherBtn->name : "button";
      errorMsg = "GPIO " + String(btn.pin) + " is already assigned to button '" + bName + "'";
      return false;
    }
  }

  if (btn.targetDevices.length() == 0) {
    errorMsg = "Please select at least one target device to control";
    return false;
  }

  btn.enabled = true;
  configureButtonPin(btn);
  _buttons.push_back(btn);
  saveToNvs();

  Serial.printf("[BUTTON-ADMIN] Added new physical button '%s' on GPIO %d -> controls '%s'\n",
                btn.name.c_str(), btn.pin, btn.targetDevices.c_str());
  return true;
}

bool ButtonManager::updateButton(PhysicalButton updatedBtn, String& errorMsg) {
  if (updatedBtn.name.length() < 2) {
    errorMsg = "Button name must be at least 2 characters";
    return false;
  }

  if (!isSafeInputPin(updatedBtn.pin)) {
    errorMsg = "GPIO " + String(updatedBtn.pin) + " is not a valid or safe input pin";
    return false;
  }

  if (updatedBtn.pin >= 0) {
    if (DeviceManager::getInstance().isPinUsedByDevice(updatedBtn.pin)) {
      Device* dev = DeviceManager::getInstance().getDeviceByPin(updatedBtn.pin);
      String dName = dev ? dev->name : "device";
      errorMsg = "GPIO " + String(updatedBtn.pin) + " is already assigned to device '" + dName + "'";
      return false;
    }
    if (isPinUsedByButton(updatedBtn.pin, updatedBtn.id)) {
      PhysicalButton* otherBtn = getButtonByPin(updatedBtn.pin);
      String bName = otherBtn ? otherBtn->name : "button";
      errorMsg = "GPIO " + String(updatedBtn.pin) + " is already assigned to button '" + bName + "'";
      return false;
    }
  }

  if (updatedBtn.targetDevices.length() == 0) {
    errorMsg = "Please select at least one target device to control";
    return false;
  }

  for (auto& b : _buttons) {
    if (b.id == updatedBtn.id) {
      if (b.pin != updatedBtn.pin && b.pin >= 0) {
        deconfigureButtonPin(b.pin);
      }
      b.name = updatedBtn.name;
      b.pin = updatedBtn.pin;
      b.mode = updatedBtn.mode;
      b.pull = updatedBtn.pull;
      b.targetDevices = updatedBtn.targetDevices;
      b.enabled = updatedBtn.enabled;

      if (b.enabled) {
        configureButtonPin(b);
      } else if (b.pin >= 0) {
        deconfigureButtonPin(b.pin);
      }

      saveToNvs();
      Serial.printf("[BUTTON-ADMIN] Updated physical button '%s' on GPIO %d\n",
                    b.name.c_str(), b.pin);
      return true;
    }
  }

  errorMsg = "Button ID not found";
  return false;
}

bool ButtonManager::deleteButton(const String& btnId, String& errorMsg) {
  for (auto it = _buttons.begin(); it != _buttons.end(); ++it) {
    if (it->id == btnId) {
      deconfigureButtonPin(it->pin);
      Serial.printf("[BUTTON-ADMIN] Deleted physical button '%s' (GPIO %d)\n",
                    it->name.c_str(), it->pin);
      _buttons.erase(it);
      saveToNvs();
      return true;
    }
  }
  errorMsg = "Button ID not found";
  return false;
}

bool ButtonManager::triggerButton(const String& btnId, String& errorMsg) {
  for (const auto& b : _buttons) {
    if (b.id == btnId) {
      executeAction(b, TRIGGER_TOGGLE);
      return true;
    }
  }
  errorMsg = "Button not found";
  return false;
}

PhysicalButton* ButtonManager::getButton(const String& btnId) {
  for (auto& b : _buttons) {
    if (b.id == btnId) return &b;
  }
  return nullptr;
}

PhysicalButton* ButtonManager::getButtonByPin(int pin) {
  if (pin < 0) return nullptr;
  for (auto& b : _buttons) {
    if (b.pin == pin) return &b;
  }
  return nullptr;
}

String ButtonManager::getButtonsJson() {
  String json = "[";
  bool first = true;
  for (const auto& b : _buttons) {
    if (!first) json += ",";
    first = false;
    json += "{";
    json += "\"id\":\"" + b.id + "\",";
    json += "\"name\":\"" + b.name + "\",";
    json += "\"pin\":" + String(b.pin) + ",";
    json += "\"mode\":" + String((int)b.mode) + ",";
    json += "\"pull\":" + String((int)b.pull) + ",";
    json += "\"targetDevices\":\"" + b.targetDevices + "\",";
    json += "\"enabled\":" + String(b.enabled ? "true" : "false") + ",";
    int currentLevel = (b.pin >= 0) ? digitalRead(b.pin) : -1;
    json += "\"pinLevel\":" + String(currentLevel);
    json += "}";
  }
  json += "]";
  return json;
}
