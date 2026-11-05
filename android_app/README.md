# SmartHome Hub - Android Integration Guide

This SmartHome Hub supports two seamless ways to run on Android devices:

---

## Option 1: Instant PWA (Progressive Web App - Recommended)
No Android Studio or APK installation required!

1. Connect your Android phone to:
   - The ESP32 SoftAP: **`SmartHome-Hub-XXXX`** (Password: `12345678`), OR
   - Your home Wi-Fi network (if already configured).
2. Open Google Chrome on your Android phone and navigate to:
   - **`http://192.168.4.1`** (Direct AP), or
   - **`http://smarthome.local`** (Home LAN)
3. In Chrome, tap the **three dots menu (⋮)** in the top right corner.
4. Select **"Install App"** or **"Add to Home screen"**.
5. The SmartHome Hub icon will be added to your Android app drawer and home screen. When launched, it opens in full-screen standalone mode without any browser URL bar, behaving exactly like a native app.

---

## Option 2: Native Android WebView APK Project

The `android_app/` folder contains a full Android Studio project that wraps the SmartHome Hub into a standalone native `.apk`:

### Features:
- **Automatic Discovery**: Automatically checks `http://smarthome.local/` and `http://192.168.4.1/`.
- **Custom IP Configuration**: Allows saving a custom IP if assigned by your router.
- **Offline & Pull-to-Refresh**: Integrated swipe down to refresh and connection retry handler.
- **Session Persistence**: Keeps your login token stored safely in Android WebView cookie storage.

### How to Build the APK:
1. Open **Android Studio**.
2. Select **File -> Open...** and select the `android_app` directory.
3. Allow Gradle to sync.
4. Click **Build -> Build Bundle(s) / APK(s) -> Build APK(s)**.
5. Transfer the generated `.apk` to your phone and install it.
