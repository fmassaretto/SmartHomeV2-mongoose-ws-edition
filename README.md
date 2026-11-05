# SmartHome Hub v2.0 (ESP32 + Mongoose.ws Edition)

A high-performance, production-grade Smart Home automation hub for ESP32 powered by the **Cesanta Mongoose Embedded Web Server**.

---

## ✨ Features

- **Multi-User Authentication & Granular Permissions (RBAC)**:
  - Each family member has their own login account (`username` and `password`).
  - Strict room-by-room access control (e.g. Alice can toggle lights in Bedroom 1 & Living Room, but cannot switch or see Bedroom 2).
  - Admin management panel to add, edit permissions, and delete user profiles.
  - All accounts and permissions persist in ESP32 NVS non-volatile flash memory across power loss.
- **WiFi Combination Mode (AP + STA)**:
  - Simultaneously operates as an Access Point (**`SmartHome-Hub-XXXX`**, default IP `192.168.4.1`) AND connects as a Station to your home Wi-Fi.
  - If your home router is offline or credentials are not yet set, the AP remains permanently available for direct control and configuration.
  - Wi-Fi network scanner and credential saving via Web UI.
- **Real-Time WebSocket Sync**:
  - Powered by Mongoose WebSockets (`/ws`).
  - Device status and sensor readings update instantaneously across all connected browsers and Android phones without page reloads.
- **Dual-Partition Over-The-Air (OTA) Updates**:
  - Web UI firmware updater with real-time streaming progress bar and safe automatic reboot.
  - Zero RAM overflow risk using chunked streaming directly to the passive flash partition.
- **Modern Responsive Web UI & PWA**:
  - Embedded directly into flash memory—zero CDN dependency, works 100% offline.
  - Glassmorphism design, dark/light theme, toggle cards, smooth dimmer sliders, live sensor monitors.
  - Progressive Web App (PWA) manifest support—can be installed to Android home screen like a native app.
- **Dedicated Android App**:
  - Full native Android Studio WebView project in [`android_app/`](file:///d:/Programming/Projects/ESP32-8266/SmartHomeV2%20mongoose%20ws%20edition/android_app).

---

## 🔐 Default Access Profiles

| Profile | Username | Password | Role | Allowed Rooms |
|---|---|---|---|---|
| **Administrator** | `admin` | `admin123` | Admin | Full Access (All Rooms, User Management, WiFi, OTA) |
| **Alice (Child / Master)** | `alice` | `alice123` | User | `bedroom_1`, `living_room` |
| **Bob (Child / Guest)** | `bob` | `bob123` | User | `bedroom_2`, `living_room` |

*(Additional users and custom permissions can be added from the Admin panel).*

---

## 🌐 How to Connect

1. **Option A: Direct Access Point (SoftAP)**:
   - Connect your phone/laptop to Wi-Fi: **`SmartHome-Hub-XXXX`**
   - Password: **`12345678`**
   - Open browser: **`http://192.168.4.1`**
2. **Option B: Local Home Network (mDNS)**:
   - Once connected to your home router, navigate to: **`http://smarthome.local`** (or the ESP32's assigned DHCP IP).

---

## 🛠️ Hardware GPIO Mapping

| Device | Room | Type | ESP32 GPIO |
|---|---|---|---|
| Living Room Main | Living Room | Switch | `GPIO 2` (Built-in Blue LED) |
| Chandelier | Living Room | Dimmer (PWM) | `GPIO 19` |
| Ceiling Fan | Living Room | Switch / Relay | `GPIO 21` |
| Master Bed Light | Bedroom 1 | Switch | `GPIO 4` |
| Kid's Bed Light | Bedroom 2 | Switch | `GPIO 16` |
| Kitchen Downlight | Kitchen | Switch | `GPIO 17` |
| Balcony Accent | Balcony | Switch | `GPIO 18` |

*(All devices function virtually even if physical relays/sensors are not yet wired).*

---

## 🚀 Building & Flashing

Using PlatformIO:

```bash
# Build firmware binary
pio run -e esp32devMongoose

# Flash to connected ESP32 via USB
pio run -e esp32devMongoose --target upload

# Open Serial Monitor (115200 baud)
pio device monitor -b 115200
```
