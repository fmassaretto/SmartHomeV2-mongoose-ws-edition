#ifndef SMARTHOME_WEB_UI_H
#define SMARTHOME_WEB_UI_H

#include <Arduino.h>

// Web App Manifest for Android PWA (Add to Home Screen)
const char MANIFEST_JSON[] PROGMEM = R"rawliteral({
  "name": "SmartHome Hub",
  "short_name": "SmartHome",
  "start_url": "/",
  "display": "standalone",
  "background_color": "#0f172a",
  "theme_color": "#2563eb",
  "orientation": "portrait-primary",
  "icons": [
    {
      "src": "data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'%3E%3Crect width='100' height='100' rx='20' fill='%232563eb'/%3E%3Cpath d='M50 20 L20 48 L30 48 L30 78 L45 78 L45 60 L55 60 L55 78 L70 78 L70 48 L80 48 Z' fill='white'/%3E%3C/svg%3E",
      "sizes": "192x192 512x512",
      "type": "image/svg+xml"
    }
  ]
})rawliteral";

// Embedded Modern Single Page Application
const char INDEX_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>SmartHome Hub ESP32</title>
  <link rel="manifest" href="/manifest.json">
  <meta name="theme-color" content="#2563eb">
  <meta name="mobile-web-app-capable" content="yes">
  <meta name="apple-mobile-web-app-capable" content="yes">
  <meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
  <style>
    :root {
      --bg: #0b1120;
      --card-bg: rgba(30, 41, 59, 0.7);
      --card-border: rgba(255, 255, 255, 0.08);
      --card-active: rgba(37, 99, 235, 0.15);
      --text: #f8fafc;
      --text-muted: #94a3b8;
      --primary: #3b82f6;
      --primary-hover: #2563eb;
      --success: #10b981;
      --danger: #ef4444;
      --warning: #f59e0b;
      --radius: 16px;
      --transition: all 0.25s cubic-bezier(0.4, 0, 0.2, 1);
    }
    .light-theme {
      --bg: #f1f5f9;
      --card-bg: rgba(255, 255, 255, 0.9);
      --card-border: rgba(0, 0, 0, 0.08);
      --card-active: rgba(37, 99, 235, 0.1);
      --text: #0f172a;
      --text-muted: #64748b;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; -webkit-tap-highlight-color: transparent; }
    body { background-color: var(--bg); color: var(--text); min-height: 100vh; display: flex; flex-direction: column; overflow-x: hidden; }
    
    /* Header */
    header {
      background: var(--card-bg);
      backdrop-filter: blur(16px);
      border-bottom: 1px solid var(--card-border);
      padding: 12px 20px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      position: sticky;
      top: 0;
      z-index: 50;
    }
    .brand { display: flex; align-items: center; gap: 10px; font-weight: 700; font-size: 1.15rem; }
    .brand svg { width: 28px; height: 28px; fill: var(--primary); }
    .header-actions { display: flex; align-items: center; gap: 10px; }
    .badge {
      display: inline-flex; align-items: center; gap: 5px; padding: 4px 10px;
      border-radius: 9999px; font-size: 0.75rem; font-weight: 600;
    }
    .badge-live { background: rgba(16, 185, 129, 0.2); color: var(--success); }
    .badge-user { background: rgba(59, 130, 246, 0.2); color: var(--primary); }
    .btn-icon { background: none; border: none; color: var(--text); cursor: pointer; padding: 8px; border-radius: 50%; display: flex; align-items: center; justify-content: center; }
    .btn-icon:hover { background: var(--card-border); }

    /* Layout */
    .container { max-width: 1100px; margin: 0 auto; padding: 16px; width: 100%; flex: 1; }
    
    /* Auth Modal / Box */
    .auth-box {
      max-width: 420px; margin: 40px auto; background: var(--card-bg); backdrop-filter: blur(20px);
      border: 1px solid var(--card-border); border-radius: var(--radius); padding: 32px 24px;
      box-shadow: 0 20px 25px -5px rgba(0, 0, 0, 0.3); text-align: center;
    }
    .auth-box h2 { font-size: 1.75rem; margin-bottom: 8px; }
    .auth-box p { color: var(--text-muted); font-size: 0.9rem; margin-bottom: 24px; }
    .form-group { text-align: left; margin-bottom: 16px; }
    .form-group label { display: block; font-size: 0.8rem; font-weight: 600; color: var(--text-muted); margin-bottom: 6px; text-transform: uppercase; letter-spacing: 0.5px; }
    .input-field {
      width: 100%; padding: 12px 14px; background: rgba(0, 0, 0, 0.2); border: 1px solid var(--card-border);
      border-radius: 10px; color: var(--text); font-size: 1rem; outline: none; transition: var(--transition);
    }
    .light-theme .input-field { background: #fff; }
    .input-field:focus { border-color: var(--primary); box-shadow: 0 0 0 2px rgba(59, 130, 246, 0.3); }
    .btn {
      width: 100%; padding: 12px; border-radius: 10px; font-weight: 600; font-size: 1rem; cursor: pointer;
      border: none; background: var(--primary); color: #fff; transition: var(--transition); display: flex;
      align-items: center; justify-content: center; gap: 8px;
    }
    .btn:hover { background: var(--primary-hover); }
    .btn-outline { background: transparent; border: 1px solid var(--card-border); color: var(--text); }
    .btn-outline:hover { background: var(--card-border); }
    .btn-danger { background: var(--danger); }
    .btn-sm { padding: 6px 12px; font-size: 0.85rem; width: auto; }
    
    .quick-users { margin-top: 24px; border-top: 1px solid var(--card-border); padding-top: 16px; text-align: left; }
    .quick-users span { font-size: 0.75rem; color: var(--text-muted); text-transform: uppercase; font-weight: 600; }
    .quick-pill {
      display: inline-block; padding: 6px 12px; margin: 4px; background: rgba(255, 255, 255, 0.05);
      border: 1px solid var(--card-border); border-radius: 8px; font-size: 0.8rem; cursor: pointer;
      color: var(--text-muted); transition: var(--transition);
    }
    .quick-pill:hover { border-color: var(--primary); color: var(--text); }

    /* Stats Grid */
    .stats-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(130px, 1fr)); gap: 12px; margin-bottom: 20px; }
    .stat-card {
      background: var(--card-bg); border: 1px solid var(--card-border); border-radius: var(--radius);
      padding: 14px; display: flex; flex-direction: column; gap: 4px; backdrop-filter: blur(12px);
    }
    .stat-card .label { font-size: 0.75rem; color: var(--text-muted); font-weight: 600; text-transform: uppercase; }
    .stat-card .val { font-size: 1.4rem; font-weight: 700; color: var(--text); }

    /* Nav Pills */
    .nav-pills { display: flex; gap: 8px; overflow-x: auto; padding-bottom: 8px; margin-bottom: 20px; scrollbar-width: none; }
    .nav-pills::-webkit-scrollbar { display: none; }
    .pill {
      padding: 8px 18px; border-radius: 9999px; background: var(--card-bg); border: 1px solid var(--card-border);
      color: var(--text-muted); font-size: 0.85rem; font-weight: 600; cursor: pointer; white-space: nowrap; transition: var(--transition);
    }
    .pill.active { background: var(--primary); color: #fff; border-color: var(--primary); box-shadow: 0 4px 12px rgba(59, 130, 246, 0.35); }

    /* Device Grid */
    .device-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(240px, 1fr)); gap: 16px; margin-bottom: 30px; }
    .device-card {
      background: var(--card-bg); border: 1px solid var(--card-border); border-radius: var(--radius);
      padding: 18px; display: flex; flex-direction: column; justify-content: space-between;
      backdrop-filter: blur(12px); transition: var(--transition); position: relative; min-height: 150px;
    }
    .device-card.active { border-color: rgba(59, 130, 246, 0.4); background: var(--card-active); }
    .device-card:hover { transform: translateY(-2px); }
    .device-top { display: flex; justify-content: space-between; align-items: flex-start; margin-bottom: 12px; }
    .device-icon-wrap {
      width: 44px; height: 44px; border-radius: 12px; display: flex; align-items: center; justify-content: center;
      background: rgba(255, 255, 255, 0.05); color: var(--text-muted); transition: var(--transition);
    }
    .device-card.active .device-icon-wrap { background: var(--primary); color: #fff; box-shadow: 0 0 16px rgba(59, 130, 246, 0.6); }
    .device-info h4 { font-size: 1rem; font-weight: 600; margin-bottom: 2px; }
    .device-info span { font-size: 0.75rem; color: var(--text-muted); text-transform: capitalize; }
    
    /* Toggle Switch */
    .switch { position: relative; display: inline-block; width: 48px; height: 26px; }
    .switch input { opacity: 0; width: 0; height: 0; }
    .slider {
      position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0;
      background-color: rgba(255, 255, 255, 0.15); transition: .3s; border-radius: 34px;
    }
    .slider:before {
      position: absolute; content: ""; height: 20px; width: 20px; left: 3px; bottom: 3px;
      background-color: white; transition: .3s; border-radius: 50%;
    }
    input:checked + .slider { background-color: var(--primary); }
    input:checked + .slider:before { transform: translateX(22px); }

    /* Dimmer Slider */
    .range-wrap { margin-top: 14px; }
    .range-slider {
      -webkit-appearance: none; width: 100%; height: 6px; border-radius: 3px; background: rgba(255, 255, 255, 0.15);
      outline: none; transition: background 450ms ease-in;
    }
    .range-slider::-webkit-slider-thumb {
      -webkit-appearance: none; appearance: none; width: 18px; height: 18px; border-radius: 50%;
      background: var(--primary); cursor: pointer; box-shadow: 0 0 8px rgba(59, 130, 246, 0.8);
    }

    /* Admin Panels */
    .admin-section {
      background: var(--card-bg); border: 1px solid var(--card-border); border-radius: var(--radius);
      padding: 24px; margin-bottom: 24px; backdrop-filter: blur(12px);
    }
    .admin-section h3 { font-size: 1.25rem; margin-bottom: 16px; display: flex; align-items: center; justify-content: space-between; }
    .user-table { width: 100%; border-collapse: collapse; text-align: left; font-size: 0.9rem; }
    .user-table th { padding: 10px; color: var(--text-muted); border-bottom: 1px solid var(--card-border); font-size: 0.75rem; text-transform: uppercase; }
    .user-table td { padding: 12px 10px; border-bottom: 1px solid var(--card-border); }
    .actions-cell { display: flex; gap: 8px; }

    /* Modal */
    .modal-overlay {
      position: fixed; top: 0; left: 0; right: 0; bottom: 0; background: rgba(0, 0, 0, 0.7);
      backdrop-filter: blur(8px); display: none; align-items: center; justify-content: center; z-index: 100; padding: 16px;
    }
    .modal {
      background: var(--card-bg); border: 1px solid var(--card-border); border-radius: var(--radius);
      width: 100%; max-width: 480px; padding: 24px; box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.5);
    }
    .modal-title { font-size: 1.25rem; margin-bottom: 16px; font-weight: 700; }
    .checkbox-group { display: grid; grid-template-columns: 1fr 1fr; gap: 8px; margin-top: 8px; }
    .checkbox-item { display: flex; align-items: center; gap: 8px; font-size: 0.85rem; cursor: pointer; }

    /* Bottom Nav for Mobile / PWA */
    .bottom-nav {
      display: none; position: fixed; bottom: 0; left: 0; right: 0; background: var(--card-bg);
      backdrop-filter: blur(20px); border-top: 1px solid var(--card-border); padding: 8px 16px;
      justify-content: space-around; z-index: 40;
    }
    .nav-item {
      display: flex; flex-direction: column; align-items: center; gap: 4px; color: var(--text-muted);
      font-size: 0.7rem; font-weight: 600; cursor: pointer; text-decoration: none;
    }
    .nav-item.active { color: var(--primary); }
    .nav-item svg { width: 22px; height: 22px; }

    @media (max-width: 640px) {
      .bottom-nav { display: flex; }
      body { padding-bottom: 70px; }
      .brand span { font-size: 1rem; }
      .container { padding: 12px; }
    }

    /* Animation */
    .spinning { animation: spin 2s linear infinite; }
    @keyframes spin { from { transform: rotate(0deg); } to { transform: rotate(360deg); } }
  </style>
</head>
<body>

  <!-- Header -->
  <header>
    <div class="brand">
      <svg viewBox="0 0 24 24"><path d="M10 20v-6h4v6h5v-8h3L12 3 2 12h3v8z"/></svg>
      <span>SmartHome<span style="color:var(--primary);">Hub</span></span>
    </div>
    <div class="header-actions">
      <span id="wsBadge" class="badge badge-live">● Live</span>
      <span id="userBadge" class="badge badge-user" style="display:none;"></span>
      <button class="btn-icon" id="themeToggle" title="Toggle Theme">
        <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="5"/><path d="M12 1v2M12 21v2M4.22 4.22l1.42 1.42M18.36 18.36l1.42 1.42M1 12h2M21 12h2M4.22 19.78l1.42-1.42M18.36 5.64l1.42-1.42"/></svg>
      </button>
      <button class="btn-icon" id="logoutBtn" style="display:none;" title="Logout">
        <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M9 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h4"/><polyline points="16 17 21 12 16 7"/><line x1="21" y1="12" x2="9" y2="12"/></svg>
      </button>
    </div>
  </header>

  <!-- Main Content Container -->
  <div class="container">

    <!-- LOGIN SCREEN -->
    <div id="loginSection" class="auth-box">
      <h2>Welcome Home</h2>
      <p>Log in to access your room and device controls</p>
      <form id="loginForm">
        <div class="form-group">
          <label>Username</label>
          <input type="text" id="loginUsername" class="input-field" placeholder="e.g. alice, bob, admin" required>
        </div>
        <div class="form-group">
          <label>Password</label>
          <input type="password" id="loginPassword" class="input-field" placeholder="••••••••" required>
        </div>
        <button type="submit" class="btn" id="loginSubmitBtn">
          <span>Sign In</span>
          <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M5 12h14M12 5l7 7-7 7"/></svg>
        </button>
      </form>
      <div id="loginError" style="color:var(--danger); font-size:0.85rem; margin-top:12px; display:none;"></div>
      
      <div class="quick-users">
        <span>Quick Profiles (Demo Setup):</span>
        <div style="margin-top:6px;">
          <div class="quick-pill" onclick="quickFill('admin','admin123')">🔑 Admin (Full Access)</div>
          <div class="quick-pill" onclick="quickFill('alice','alice123')">🛏️ Alice (Bed 1 + Living)</div>
          <div class="quick-pill" onclick="quickFill('bob','bob123')">🛏️ Bob (Bed 2 + Living)</div>
        </div>
      </div>
    </div>

    <!-- DASHBOARD VIEW -->
    <div id="dashboardSection" style="display:none;">
      
      <!-- Real-time Sensors Header -->
      <div class="stats-grid">
        <div class="stat-card">
          <span class="label">Temperature</span>
          <span class="val" id="statTemp">-- °C</span>
        </div>
        <div class="stat-card">
          <span class="label">Humidity</span>
          <span class="val" id="statHum">-- %</span>
        </div>
        <div class="stat-card">
          <span class="label">Motion</span>
          <span class="val" id="statMotion" style="font-size:1.1rem; color:var(--text-muted);">Clear</span>
        </div>
        <div class="stat-card">
          <span class="label">Power Load</span>
          <span class="val" id="statPower">-- W</span>
        </div>
        <div class="stat-card">
          <span class="label">Active Devices</span>
          <span class="val" id="statActive">0</span>
        </div>
      </div>

      <!-- Room Navigation Pills -->
      <div class="nav-pills" id="roomPills">
        <div class="pill active" onclick="filterRoom('all')">All Allowed</div>
      </div>

      <!-- Devices Grid -->
      <div class="device-grid" id="devicesContainer">
        <!-- Rendered dynamically -->
      </div>

      <!-- ADMIN ONLY SETTINGS VIEW -->
      <div id="adminPanel" style="display:none;">
        
        <!-- User Management -->
        <div class="admin-section">
          <h3>
            <span>House Profiles & Permissions</span>
            <button class="btn btn-sm" onclick="openAddUserModal()">+ Add Person</button>
          </h3>
          <p style="color:var(--text-muted); font-size:0.85rem; margin-bottom:16px;">
            Configure which rooms each family member can control from their phone or browser.
          </p>
          <div style="overflow-x:auto;">
            <table class="user-table">
              <thead>
                <tr>
                  <th>Person</th>
                  <th>Username</th>
                  <th>Role</th>
                  <th>Allowed Rooms</th>
                  <th>Actions</th>
                </tr>
              </thead>
              <tbody id="userTableBody">
                <!-- Rendered dynamically -->
              </tbody>
            </table>
          </div>
        </div>

        <!-- WiFi Combination Mode Settings -->
        <div class="admin-section">
          <h3>WiFi Combination Mode (AP + STA)</h3>
          <p style="color:var(--text-muted); font-size:0.85rem; margin-bottom:16px;">
            ESP32 broadcasts its own Access Point for direct connection, and connects to your home router.
          </p>
          <div style="display:grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap:16px;">
            <div>
              <div class="form-group">
                <label>SoftAP Network (Direct Fallback)</label>
                <input type="text" id="wifiApSsid" class="input-field" readonly>
              </div>
              <div class="form-group">
                <label>SoftAP IP</label>
                <input type="text" id="wifiApIp" class="input-field" readonly>
              </div>
              <button class="btn btn-outline" onclick="scanWifi()">Scan Home Wi-Fi Networks</button>
              <div id="scanResults" style="margin-top:10px; max-height:160px; overflow-y:auto;"></div>
            </div>
            <div>
              <form id="wifiForm">
                <div class="form-group">
                  <label>Home Router SSID</label>
                  <input type="text" id="wifiStaSsid" class="input-field" placeholder="Enter home Wi-Fi name" required>
                </div>
                <div class="form-group">
                  <label>Home Router Password</label>
                  <input type="password" id="wifiStaPass" class="input-field" placeholder="Enter home Wi-Fi password">
                </div>
                <button type="submit" class="btn">Connect & Save to ESP32 Flash</button>
              </form>
            </div>
          </div>
        </div>

        <!-- System Stats & OTA Firmware Update -->
        <div class="admin-section">
          <h3>System & OTA Firmware Update</h3>
          <div style="display:grid; grid-template-columns: repeat(auto-fit, minmax(240px, 1fr)); gap:16px; margin-bottom:16px;">
            <div>
              <p style="font-size:0.85rem; color:var(--text-muted);">Uptime: <strong id="sysUptime" style="color:var(--text)">--</strong></p>
              <p style="font-size:0.85rem; color:var(--text-muted);">Free RAM: <strong id="sysHeap" style="color:var(--text)">--</strong></p>
              <p style="font-size:0.85rem; color:var(--text-muted);">Active Partition: <strong id="sysOta" style="color:var(--text)">--</strong></p>
              <p style="font-size:0.85rem; color:var(--text-muted);">Firmware Version: <strong id="sysVer" style="color:var(--text)">2.0.0</strong></p>
              <button class="btn btn-outline btn-sm" style="margin-top:10px;" onclick="rebootEsp()">Reboot ESP32</button>
            </div>
            <div>
              <label style="font-size:0.8rem; font-weight:600; color:var(--text-muted); text-transform:uppercase;">Flash New Firmware (.bin)</label>
              <input type="file" id="otaFile" accept=".bin" class="input-field" style="margin:8px 0;">
              <button class="btn" id="otaUploadBtn" onclick="uploadOta()">Upload & Update Firmware</button>
              <div id="otaProgress" style="display:none; margin-top:10px;">
                <div style="height:6px; background:rgba(255,255,255,0.1); border-radius:3px; overflow:hidden;">
                  <div id="otaBar" style="height:100%; width:0%; background:var(--primary); transition:width 0.2s;"></div>
                </div>
                <span id="otaPercent" style="font-size:0.8rem; color:var(--text-muted); margin-top:4px; display:block;">Uploading... 0%</span>
              </div>
            </div>
          </div>
        </div>

      </div>

    </div>
  </div>

  <!-- Add/Edit User Modal -->
  <div class="modal-overlay" id="userModal">
    <div class="modal">
      <div class="modal-title" id="userModalTitle">Add New Person</div>
      <form id="userModalForm">
        <input type="hidden" id="modalIsEdit" value="0">
        <div class="form-group">
          <label>Display Name (e.g. Sister, Mom, Guest)</label>
          <input type="text" id="modalDisplayName" class="input-field" placeholder="Display Name" required>
        </div>
        <div class="form-group">
          <label>Username</label>
          <input type="text" id="modalUsername" class="input-field" placeholder="username" required>
        </div>
        <div class="form-group">
          <label>Password</label>
          <input type="password" id="modalPassword" class="input-field" placeholder="••••••••">
        </div>
        <div class="form-group">
          <label>Role</label>
          <select id="modalRole" class="input-field" onchange="toggleRoomCheckboxes()">
            <option value="user">Regular User (Permission Restricted)</option>
            <option value="admin">Administrator (Full Access)</option>
          </select>
        </div>
        <div class="form-group" id="roomPermsGroup">
          <label>Allowed Rooms</label>
          <div class="checkbox-group">
            <label class="checkbox-item"><input type="checkbox" value="living_room" class="room-chk"> Living Room</label>
            <label class="checkbox-item"><input type="checkbox" value="bedroom_1" class="room-chk"> Master Bedroom</label>
            <label class="checkbox-item"><input type="checkbox" value="bedroom_2" class="room-chk"> Kid's Bedroom</label>
            <label class="checkbox-item"><input type="checkbox" value="kitchen" class="room-chk"> Kitchen</label>
            <label class="checkbox-item"><input type="checkbox" value="balcony" class="room-chk"> Balcony & Garden</label>
          </div>
        </div>
        <div style="display:flex; gap:10px; margin-top:20px;">
          <button type="button" class="btn btn-outline" onclick="closeUserModal()">Cancel</button>
          <button type="submit" class="btn">Save Person</button>
        </div>
      </form>
    </div>
  </div>

  <!-- Bottom Nav for Mobile Screens -->
  <div class="bottom-nav" id="bottomNav" style="display:none;">
    <a class="nav-item active" onclick="switchMobileTab('controls')">
      <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M3 9l9-7 9 7v11a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/><polyline points="9 22 9 12 15 12 15 22"/></svg>
      <span>Controls</span>
    </a>
    <a class="nav-item" id="adminMobileTab" style="display:none;" onclick="switchMobileTab('admin')">
      <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1 0 2.83 2 2 0 0 1-2.83 0l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-2 2 2 2 0 0 1-2-2v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83 0 2 2 0 0 1 0-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1-2-2 2 2 0 0 1 2-2h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 0-2.83 2 2 0 0 1 2.83 0l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 2-2 2 2 0 0 1 2 2v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 0 2 2 0 0 1 0 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 2 2 2 2 0 0 1-2 2h-.09a1.65 1.65 0 0 0-1.51 1z"/></svg>
      <span>Settings</span>
    </a>
  </div>

  <script>
    // State
    let authToken = localStorage.getItem('smarthome_token') || '';
    let currentUser = null;
    let currentRoomFilter = 'all';
    let appData = { rooms: [], devices: [], sensors: {} };
    let ws = null;

    // Theme Toggle
    const themeToggleBtn = document.getElementById('themeToggle');
    if (localStorage.getItem('theme') === 'light') document.body.classList.add('light-theme');
    themeToggleBtn.addEventListener('click', () => {
      document.body.classList.toggle('light-theme');
      localStorage.setItem('theme', document.body.classList.contains('light-theme') ? 'light' : 'dark');
    });

    // Quick Fill for Login
    function quickFill(u, p) {
      document.getElementById('loginUsername').value = u;
      document.getElementById('loginPassword').value = p;
    }

    // App Initialization
    window.addEventListener('DOMContentLoaded', () => {
      if (authToken) {
        verifySession();
      } else {
        showLogin();
      }
    });

    async function verifySession() {
      try {
        const res = await fetch('/api/auth/me', {
          headers: { 'Authorization': 'Bearer ' + authToken }
        });
        if (res.ok) {
          currentUser = await res.json();
          showDashboard();
        } else {
          showLogin();
        }
      } catch (err) {
        showDashboard(); // fallback offline mode
      }
    }

    function showLogin() {
      authToken = '';
      currentUser = null;
      localStorage.removeItem('smarthome_token');
      document.getElementById('loginSection').style.display = 'block';
      document.getElementById('dashboardSection').style.display = 'none';
      document.getElementById('userBadge').style.display = 'none';
      document.getElementById('logoutBtn').style.display = 'none';
      document.getElementById('bottomNav').style.display = 'none';
      if (ws) { ws.close(); ws = null; }
    }

    function showDashboard() {
      document.getElementById('loginSection').style.display = 'none';
      document.getElementById('dashboardSection').style.display = 'block';
      
      const uBadge = document.getElementById('userBadge');
      uBadge.style.display = 'inline-flex';
      uBadge.innerText = currentUser ? (currentUser.displayName || currentUser.username) : 'User';
      document.getElementById('logoutBtn').style.display = 'inline-flex';

      const isAdmin = currentUser && currentUser.role === 'admin';
      document.getElementById('adminPanel').style.display = isAdmin ? 'block' : 'none';
      document.getElementById('adminMobileTab').style.display = isAdmin ? 'flex' : 'none';
      document.getElementById('bottomNav').style.display = 'flex';

      loadDevices();
      if (isAdmin) {
        loadUsers();
        loadSystemStats();
      }
      initWebSocket();
    }

    // Login Form Submit
    document.getElementById('loginForm').addEventListener('submit', async (e) => {
      e.preventDefault();
      const u = document.getElementById('loginUsername').value.trim();
      const p = document.getElementById('loginPassword').value.trim();
      const errBox = document.getElementById('loginError');
      errBox.style.display = 'none';

      try {
        const res = await fetch('/api/auth/login', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ username: u, password: p })
        });
        const data = await res.json();
        if (res.ok && data.token) {
          authToken = data.token;
          currentUser = data.user;
          localStorage.setItem('smarthome_token', authToken);
          showDashboard();
        } else {
          errBox.innerText = data.error || 'Invalid username or password';
          errBox.style.display = 'block';
        }
      } catch (err) {
        errBox.innerText = 'Cannot connect to ESP32 Hub';
        errBox.style.display = 'block';
      }
    });

    document.getElementById('logoutBtn').addEventListener('click', async () => {
      try {
        await fetch('/api/auth/logout', {
          method: 'POST',
          headers: { 'Authorization': 'Bearer ' + authToken }
        });
      } catch (e) {}
      showLogin();
    });

    // WebSocket Management
    function initWebSocket() {
      if (ws) return;
      const protocol = location.protocol === 'https:' ? 'wss:' : 'ws:';
      const wsUrl = `${protocol}//${location.host}/ws`;
      ws = new WebSocket(wsUrl);

      ws.onopen = () => {
        document.getElementById('wsBadge').className = 'badge badge-live';
        document.getElementById('wsBadge').innerText = '● Live';
      };

      ws.onmessage = (event) => {
        try {
          const msg = JSON.parse(event.data);
          if (msg.event === 'device_update') {
            const d = msg.data;
            updateDeviceUi(d.deviceId, d.state, d.value);
          } else if (msg.event === 'sensors_update') {
            renderSensors(msg.data);
          }
        } catch (e) {}
      };

      ws.onclose = () => {
        document.getElementById('wsBadge').className = 'badge';
        document.getElementById('wsBadge').style.background = 'rgba(239,68,68,0.2)';
        document.getElementById('wsBadge').style.color = 'var(--danger)';
        document.getElementById('wsBadge').innerText = '○ Offline';
        ws = null;
        setTimeout(initWebSocket, 3000);
      };
    }

    // Load Devices from Server
    async function loadDevices() {
      try {
        const res = await fetch('/api/devices', {
          headers: { 'Authorization': 'Bearer ' + authToken }
        });
        if (res.ok) {
          appData = await res.json();
          renderRoomPills();
          renderDevices();
          renderSensors(appData.sensors);
        }
      } catch (err) {
        console.error('Failed to load devices', err);
      }
    }

    function renderRoomPills() {
      const container = document.getElementById('roomPills');
      let html = `<div class="pill ${currentRoomFilter === 'all' ? 'active' : ''}" onclick="filterRoom('all')">All Allowed</div>`;
      appData.rooms.forEach(r => {
        html += `<div class="pill ${currentRoomFilter === r.id ? 'active' : ''}" onclick="filterRoom('${r.id}')">${r.name}</div>`;
      });
      container.innerHTML = html;
    }

    function filterRoom(roomId) {
      currentRoomFilter = roomId;
      renderRoomPills();
      renderDevices();
    }

    function renderDevices() {
      const container = document.getElementById('devicesContainer');
      const filtered = appData.devices.filter(d => currentRoomFilter === 'all' || d.roomId === currentRoomFilter);
      
      let activeCount = 0;
      let html = '';
      filtered.forEach(d => {
        if (d.state) activeCount++;
        const isDimmer = d.type === 1;
        const isFan = d.type === 2;

        html += `
          <div class="device-card ${d.state ? 'active' : ''}" id="card_${d.id}">
            <div class="device-top">
              <div class="device-icon-wrap">
                ${isFan ? 
                  `<svg class="${d.state ? 'spinning' : ''}" width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M12 12c0-3 2.5-5 5-5s3 2 3 4-2 3.5-5 3.5M12 12c-3 0-5-2.5-5-5s2-3 4-3 3.5 2 3.5 5M12 12c0 3-2.5 5-5 5s-3-2-3-4 2-3.5 5-3.5M12 12c3 0 5 2.5 5 5s-2 3-4 3-3.5-2-3.5-5"/><circle cx="12" cy="12" r="2"/></svg>` :
                  `<svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M9 18h6M10 22h4M12 2a7 7 0 0 0-7 7c0 2.38 1.19 4.47 3 5.74V17a1 1 0 0 0 1 1h6a1 1 0 0 0 1-1v-2.26c1.81-1.27 3-3.36 3-5.74a7 7 0 0 0-7-7z"/></svg>`
                }
              </div>
              <label class="switch">
                <input type="checkbox" id="chk_${d.id}" ${d.state ? 'checked' : ''} onchange="toggleDevice('${d.id}', this.checked)">
                <span class="slider"></span>
              </label>
            </div>
            <div class="device-info">
              <h4>${d.name}</h4>
              <span>${d.roomId.replace('_', ' ')}</span>
            </div>
            ${isDimmer ? `
              <div class="range-wrap">
                <div style="display:flex; justify-content:space-between; font-size:0.75rem; color:var(--text-muted); margin-bottom:4px;">
                  <span>Brightness</span>
                  <span id="val_${d.id}">${d.value}%</span>
                </div>
                <input type="range" min="1" max="100" value="${d.value}" class="range-slider" 
                       oninput="document.getElementById('val_${d.id}').innerText = this.value + '%'"
                       onchange="setDimmerValue('${d.id}', this.value)">
              </div>
            ` : ''}
          </div>
        `;
      });

      document.getElementById('statActive').innerText = activeCount;
      if (filtered.length === 0) {
        container.innerHTML = `<div style="grid-column: 1/-1; text-align:center; color:var(--text-muted); padding:40px;">No devices available in this room for your account.</div>`;
      } else {
        container.innerHTML = html;
      }
    }

    function renderSensors(s) {
      if (!s) return;
      document.getElementById('statTemp').innerText = (s.temperature || '--') + ' °C';
      document.getElementById('statHum').innerText = (s.humidity || '--') + ' %';
      
      const motionEl = document.getElementById('statMotion');
      if (s.motion) {
        motionEl.innerText = 'Detected!';
        motionEl.style.color = 'var(--danger)';
      } else {
        motionEl.innerText = 'Clear';
        motionEl.style.color = 'var(--text-muted)';
      }

      document.getElementById('statPower').innerText = (s.powerWatts || '--') + ' W';
    }

    function updateDeviceUi(deviceId, state, value) {
      const card = document.getElementById('card_' + deviceId);
      const chk = document.getElementById('chk_' + deviceId);
      if (card && chk) {
        chk.checked = state;
        if (state) card.classList.add('active');
        else card.classList.remove('active');
      }
      const valEl = document.getElementById('val_' + deviceId);
      if (valEl && value !== undefined) {
        valEl.innerText = value + '%';
      }
      // Update memory state
      const d = appData.devices.find(x => x.id === deviceId);
      if (d) {
        d.state = state;
        if (value !== undefined) d.value = value;
      }
    }

    async function toggleDevice(deviceId, state) {
      try {
        const res = await fetch('/api/devices/control', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
            'Authorization': 'Bearer ' + authToken
          },
          body: JSON.stringify({ deviceId, state })
        });
        if (!res.ok) {
          const err = await res.json();
          alert(err.error || 'Permission denied: You do not have access to this room');
          loadDevices(); // revert
        }
      } catch (e) {
        loadDevices();
      }
    }

    async function setDimmerValue(deviceId, value) {
      try {
        await fetch('/api/devices/control', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
            'Authorization': 'Bearer ' + authToken
          },
          body: JSON.stringify({ deviceId, state: true, value: parseInt(value) })
        });
      } catch (e) {}
    }

    // User Management (Admin)
    async function loadUsers() {
      try {
        const res = await fetch('/api/users', {
          headers: { 'Authorization': 'Bearer ' + authToken }
        });
        if (res.ok) {
          const users = await res.json();
          renderUserTable(users);
        }
      } catch (e) {}
    }

    function renderUserTable(users) {
      const tbody = document.getElementById('userTableBody');
      let html = '';
      users.forEach(u => {
        const isRoot = u.username.toLowerCase() === 'admin';
        html += `
          <tr>
            <td><strong>${u.displayName || u.username}</strong></td>
            <td><code>${u.username}</code></td>
            <td><span class="badge ${u.role === 'admin' ? 'badge-user' : ''}">${u.role}</span></td>
            <td><span style="font-size:0.8rem; color:var(--text-muted);">${u.allowedRooms === '*' ? 'All Rooms' : u.allowedRooms.replace(/,/g, ', ')}</span></td>
            <td class="actions-cell">
              <button class="btn btn-outline btn-sm" onclick="editUser('${u.username}','${u.displayName}','${u.role}','${u.allowedRooms}')">Edit</button>
              ${!isRoot ? `<button class="btn btn-danger btn-sm" onclick="deleteUser('${u.username}')">Delete</button>` : ''}
            </td>
          </tr>
        `;
      });
      tbody.innerHTML = html;
    }

    function openAddUserModal() {
      document.getElementById('modalIsEdit').value = '0';
      document.getElementById('userModalTitle').innerText = 'Add New Person';
      document.getElementById('modalUsername').value = '';
      document.getElementById('modalUsername').disabled = false;
      document.getElementById('modalPassword').value = '';
      document.getElementById('modalPassword').required = true;
      document.getElementById('modalDisplayName').value = '';
      document.getElementById('modalRole').value = 'user';
      document.querySelectorAll('.room-chk').forEach(c => c.checked = false);
      toggleRoomCheckboxes();
      document.getElementById('userModal').style.display = 'flex';
    }

    function editUser(username, displayName, role, rooms) {
      document.getElementById('modalIsEdit').value = '1';
      document.getElementById('userModalTitle').innerText = 'Edit Profile: ' + displayName;
      document.getElementById('modalUsername').value = username;
      document.getElementById('modalUsername').disabled = true;
      document.getElementById('modalPassword').value = '';
      document.getElementById('modalPassword').required = false;
      document.getElementById('modalDisplayName').value = displayName;
      document.getElementById('modalRole').value = role;
      
      const rArr = rooms.split(',');
      document.querySelectorAll('.room-chk').forEach(c => {
        c.checked = (rooms === '*' || rArr.includes(c.value));
      });
      toggleRoomCheckboxes();
      document.getElementById('userModal').style.display = 'flex';
    }

    function closeUserModal() {
      document.getElementById('userModal').style.display = 'none';
    }

    function toggleRoomCheckboxes() {
      const isAdm = document.getElementById('modalRole').value === 'admin';
      document.getElementById('roomPermsGroup').style.display = isAdm ? 'none' : 'block';
    }

    document.getElementById('userModalForm').addEventListener('submit', async (e) => {
      e.preventDefault();
      const isEdit = document.getElementById('modalIsEdit').value === '1';
      const username = document.getElementById('modalUsername').value.trim();
      const password = document.getElementById('modalPassword').value.trim();
      const displayName = document.getElementById('modalDisplayName').value.trim();
      const role = document.getElementById('modalRole').value;

      let allowedRooms = '*';
      if (role !== 'admin') {
        const checked = Array.from(document.querySelectorAll('.room-chk:checked')).map(c => c.value);
        if (checked.length === 0) {
          alert('Please select at least 1 room for this person.');
          return;
        }
        allowedRooms = checked.join(',');
      }

      const endpoint = isEdit ? '/api/users/update' : '/api/users/add';
      const payload = { username, displayName, role, allowedRooms };
      if (password.length > 0) payload.password = password;

      try {
        const res = await fetch(endpoint, {
          method: 'POST',
          headers: { 'Content-Type': 'application/json', 'Authorization': 'Bearer ' + authToken },
          body: JSON.stringify(payload)
        });
        const data = await res.json();
        if (res.ok) {
          closeUserModal();
          loadUsers();
        } else {
          alert(data.error || 'Failed to save person');
        }
      } catch (err) {
        alert('Network error');
      }
    });

    async function deleteUser(username) {
      if (!confirm(`Are you sure you want to delete profile '${username}'?`)) return;
      try {
        const res = await fetch('/api/users/delete', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json', 'Authorization': 'Bearer ' + authToken },
          body: JSON.stringify({ username })
        });
        if (res.ok) {
          loadUsers();
        } else {
          const d = await res.json();
          alert(d.error || 'Failed to delete');
        }
      } catch (e) {}
    }

    // System Stats & Wi-Fi Management
    async function loadSystemStats() {
      try {
        const res = await fetch('/api/system/stats', {
          headers: { 'Authorization': 'Bearer ' + authToken }
        });
        if (res.ok) {
          const s = await res.json();
          document.getElementById('sysUptime').innerText = Math.floor(s.uptime / 60) + ' min ' + (s.uptime % 60) + ' sec';
          document.getElementById('sysHeap').innerText = Math.round(s.freeHeap / 1024) + ' KB';
          document.getElementById('sysOta').innerText = s.otaPartition;
          document.getElementById('sysVer').innerText = s.version;
          
          if (s.wifi) {
            document.getElementById('wifiApSsid').value = s.wifi.ap_ssid;
            document.getElementById('wifiApIp').value = s.wifi.ap_ip;
            document.getElementById('wifiStaSsid').value = s.wifi.sta_ssid || '';
          }
        }
      } catch (e) {}
    }

    async function scanWifi() {
      const resBox = document.getElementById('scanResults');
      resBox.innerHTML = '<span style="font-size:0.8rem; color:var(--text-muted);">Scanning available networks...</span>';
      try {
        const res = await fetch('/api/wifi/scan', {
          headers: { 'Authorization': 'Bearer ' + authToken }
        });
        if (res.ok) {
          const nets = await res.json();
          let h = '';
          nets.forEach(n => {
            h += `
              <div style="padding:6px 10px; border-bottom:1px solid var(--card-border); display:flex; justify-content:space-between; cursor:pointer;" 
                   onclick="document.getElementById('wifiStaSsid').value = '${n.ssid}'">
                <span><strong>${n.ssid}</strong></span>
                <span style="font-size:0.75rem; color:var(--text-muted);">${n.rssi} dBm</span>
              </div>
            `;
          });
          resBox.innerHTML = h || '<span style="font-size:0.8rem;">No networks found</span>';
        }
      } catch (e) {
        resBox.innerHTML = '<span style="color:var(--danger); font-size:0.8rem;">Scan failed</span>';
      }
    }

    document.getElementById('wifiForm').addEventListener('submit', async (e) => {
      e.preventDefault();
      const ssid = document.getElementById('wifiStaSsid').value.trim();
      const pass = document.getElementById('wifiStaPass').value.trim();

      try {
        const res = await fetch('/api/wifi/config', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json', 'Authorization': 'Bearer ' + authToken },
          body: JSON.stringify({ ssid, password: pass })
        });
        if (res.ok) {
          alert('Wi-Fi credentials saved! ESP32 will now connect to home router in combination mode.');
        }
      } catch (e) {}
    });

    // OTA Firmware Upload
    async function uploadOta() {
      const fileInput = document.getElementById('otaFile');
      if (!fileInput.files.length) {
        alert('Please choose a .bin firmware file');
        return;
      }
      const file = fileInput.files[0];
      const progBox = document.getElementById('otaProgress');
      const bar = document.getElementById('otaBar');
      const pct = document.getElementById('otaPercent');
      progBox.style.display = 'block';

      const xhr = new XMLHttpRequest();
      xhr.open('POST', '/api/ota/upload', true);
      xhr.setRequestHeader('Authorization', 'Bearer ' + authToken);

      xhr.upload.onprogress = (e) => {
        if (e.lengthComputable) {
          const p = Math.round((e.loaded / e.total) * 100);
          bar.style.width = p + '%';
          pct.innerText = `Uploading firmware... ${p}%`;
        }
      };

      xhr.onload = () => {
        if (xhr.status === 200) {
          bar.style.width = '100%';
          pct.innerText = 'Upload complete! Rebooting ESP32 into new firmware...';
          setTimeout(() => location.reload(), 6000);
        } else {
          pct.innerText = 'Firmware update failed: ' + xhr.responseText;
        }
      };

      xhr.onerror = () => {
        pct.innerText = 'Firmware update network error';
      };

      xhr.send(file);
    }

    async function rebootEsp() {
      if (!confirm('Reboot the ESP32 Hub now?')) return;
      try {
        await fetch('/api/system/reboot', {
          method: 'POST',
          headers: { 'Authorization': 'Bearer ' + authToken }
        });
        alert('Reboot command sent. Reconnecting in 5 seconds...');
        setTimeout(() => location.reload(), 5000);
      } catch (e) {}
    }

    // Mobile Switch Tab
    function switchMobileTab(tab) {
      const c = document.getElementById('devicesContainer');
      const p = document.getElementById('roomPills');
      const a = document.getElementById('adminPanel');
      if (tab === 'controls') {
        c.style.display = 'grid';
        p.style.display = 'flex';
        a.style.display = 'none';
      } else {
        c.style.display = 'none';
        p.style.display = 'none';
        a.style.display = 'block';
      }
    }
  </script>
</body>
</html>
)rawliteral";

#endif // SMARTHOME_WEB_UI_H
