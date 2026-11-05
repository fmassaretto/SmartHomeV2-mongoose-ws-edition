#include "auth_manager.h"
#include <Preferences.h>
#include <esp_random.h>

static Preferences s_authPrefs;

AuthManager& AuthManager::getInstance() {
  static AuthManager instance;
  return instance;
}

AuthManager::AuthManager() {}

void AuthManager::initDefaultUsers() {
  _users.clear();

  // Root Admin
  User admin;
  admin.username = DEFAULT_ADMIN_USER;
  admin.password = DEFAULT_ADMIN_PASS;
  admin.displayName = DEFAULT_ADMIN_NAME;
  admin.role = "admin";
  admin.allowedRooms = "*";
  _users.push_back(admin);

  // User 1: Alice (Bedroom 1 and Living Room only)
  User alice;
  alice.username = DEFAULT_USER1_USER;
  alice.password = DEFAULT_USER1_PASS;
  alice.displayName = DEFAULT_USER1_NAME;
  alice.role = "user";
  alice.allowedRooms = DEFAULT_USER1_ROOMS;
  _users.push_back(alice);

  // User 2: Bob (Bedroom 2 and Living Room only)
  User bob;
  bob.username = DEFAULT_USER2_USER;
  bob.password = DEFAULT_USER2_PASS;
  bob.displayName = DEFAULT_USER2_NAME;
  bob.role = "user";
  bob.allowedRooms = DEFAULT_USER2_ROOMS;
  _users.push_back(bob);

  saveUsersToNvs();
  Serial.printf("[AUTH] Initialized %d default users in NVS\n", (int)_users.size());
}

void AuthManager::loadUsersFromNvs() {
  s_authPrefs.begin("smarthome_usr", false);
  int count = s_authPrefs.getInt("count", 0);

  if (count <= 0) {
    s_authPrefs.end();
    initDefaultUsers();
    return;
  }

  _users.clear();
  for (int i = 0; i < count; i++) {
    String key = "u_" + String(i);
    String raw = s_authPrefs.getString(key.c_str(), "");
    if (raw.length() == 0) continue;

    // Format: username|password|displayName|role|allowedRooms
    int p1 = raw.indexOf('|');
    int p2 = raw.indexOf('|', p1 + 1);
    int p3 = raw.indexOf('|', p2 + 1);
    int p4 = raw.indexOf('|', p3 + 1);

    if (p1 > 0 && p2 > 0 && p3 > 0 && p4 > 0) {
      User u;
      u.username = raw.substring(0, p1);
      u.password = raw.substring(p1 + 1, p2);
      u.displayName = raw.substring(p2 + 1, p3);
      u.role = raw.substring(p3 + 1, p4);
      u.allowedRooms = raw.substring(p4 + 1);
      _users.push_back(u);
    }
  }
  s_authPrefs.end();

  if (_users.empty()) {
    initDefaultUsers();
  } else {
    Serial.printf("[AUTH] Loaded %d users from NVS storage\n", (int)_users.size());
  }
}

void AuthManager::saveUsersToNvs() {
  s_authPrefs.begin("smarthome_usr", false);
  s_authPrefs.clear();
  s_authPrefs.putInt("count", (int)_users.size());

  for (size_t i = 0; i < _users.size(); i++) {
    const User& u = _users[i];
    String raw = u.username + "|" + u.password + "|" + u.displayName + "|" + u.role + "|" + u.allowedRooms;
    String key = "u_" + String(i);
    s_authPrefs.putString(key.c_str(), raw);
  }
  s_authPrefs.end();
}

void AuthManager::begin() {
  loadUsersFromNvs();
}

String AuthManager::generateToken() {
  static const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  char token[33];
  for (int i = 0; i < 32; i++) {
    token[i] = charset[esp_random() % (sizeof(charset) - 1)];
  }
  token[32] = '\0';
  return String(token);
}

void AuthManager::cleanExpiredSessions() {
  unsigned long now = millis();
  for (auto it = _sessions.begin(); it != _sessions.end();) {
    if (now - it->createdAt > SESSION_EXPIRY_MS) {
      it = _sessions.erase(it);
    } else {
      ++it;
    }
  }
}

bool AuthManager::login(const String& username, const String& password, String& outToken, User& outUser) {
  cleanExpiredSessions();

  for (const auto& u : _users) {
    if (u.username.equalsIgnoreCase(username) && u.password == password) {
      outToken = generateToken();
      outUser = u;

      Session s;
      s.token = outToken;
      s.username = u.username;
      s.createdAt = millis();
      _sessions.push_back(s);

      Serial.printf("[AUTH] User '%s' logged in successfully. Active sessions: %d\n",
                    u.username.c_str(), (int)_sessions.size());
      return true;
    }
  }

  Serial.printf("[AUTH] Failed login attempt for username '%s'\n", username.c_str());
  return false;
}

void AuthManager::logout(const String& token) {
  for (auto it = _sessions.begin(); it != _sessions.end(); ++it) {
    if (it->token == token) {
      Serial.printf("[AUTH] Session closed for user '%s'\n", it->username.c_str());
      _sessions.erase(it);
      break;
    }
  }
}

bool AuthManager::validateToken(const String& token, User& outUser) {
  if (token.length() == 0) return false;
  cleanExpiredSessions();

  for (const auto& s : _sessions) {
    if (s.token == token) {
      for (const auto& u : _users) {
        if (u.username.equalsIgnoreCase(s.username)) {
          outUser = u;
          return true;
        }
      }
    }
  }
  return false;
}

bool AuthManager::canAccessRoom(const User& user, const String& roomId) {
  if (user.isAdmin()) return true;
  if (user.allowedRooms == "*") return true;

  // Check comma-separated tokens
  int start = 0;
  while (start < (int)user.allowedRooms.length()) {
    int comma = user.allowedRooms.indexOf(',', start);
    String token = (comma == -1) ? user.allowedRooms.substring(start) : user.allowedRooms.substring(start, comma);
    token.trim();
    if (token.equalsIgnoreCase(roomId)) {
      return true;
    }
    if (comma == -1) break;
    start = comma + 1;
  }
  return false;
}

bool AuthManager::canAccessDevice(const User& user, const String& roomId, const String& deviceId) {
  (void)deviceId;
  return canAccessRoom(user, roomId);
}

std::vector<User> AuthManager::getAllUsers() {
  return _users;
}

bool AuthManager::addUser(const User& newUser, String& errorMsg) {
  if (newUser.username.length() < 3 || newUser.password.length() < 4) {
    errorMsg = "Username must be >= 3 chars, password >= 4 chars";
    return false;
  }

  for (const auto& u : _users) {
    if (u.username.equalsIgnoreCase(newUser.username)) {
      errorMsg = "Username already exists";
      return false;
    }
  }

  _users.push_back(newUser);
  saveUsersToNvs();
  Serial.printf("[AUTH] Added new user '%s' (role: %s)\n",
                newUser.username.c_str(), newUser.role.c_str());
  return true;
}

bool AuthManager::updateUser(const User& updatedUser, String& errorMsg) {
  for (auto& u : _users) {
    if (u.username.equalsIgnoreCase(updatedUser.username)) {
      if (updatedUser.password.length() >= 4) {
        u.password = updatedUser.password;
      }
      if (updatedUser.displayName.length() > 0) {
        u.displayName = updatedUser.displayName;
      }
      // Cannot revoke admin from primary admin account
      if (!u.username.equalsIgnoreCase(DEFAULT_ADMIN_USER)) {
        u.role = updatedUser.role;
        u.allowedRooms = updatedUser.allowedRooms;
      }
      saveUsersToNvs();
      Serial.printf("[AUTH] Updated user '%s'\n", u.username.c_str());
      return true;
    }
  }
  errorMsg = "User not found";
  return false;
}

bool AuthManager::deleteUser(const String& username, String& errorMsg) {
  if (username.equalsIgnoreCase(DEFAULT_ADMIN_USER)) {
    errorMsg = "Cannot delete default administrator account";
    return false;
  }

  for (auto it = _users.begin(); it != _users.end(); ++it) {
    if (it->username.equalsIgnoreCase(username)) {
      _users.erase(it);

      // Invalidate active sessions for this user
      for (auto sit = _sessions.begin(); sit != _sessions.end();) {
        if (sit->username.equalsIgnoreCase(username)) {
          sit = _sessions.erase(sit);
        } else {
          ++sit;
        }
      }

      saveUsersToNvs();
      Serial.printf("[AUTH] Deleted user '%s'\n", username.c_str());
      return true;
    }
  }
  errorMsg = "User not found";
  return false;
}

String AuthManager::getUserProfileJson(const User& user) {
  String json = "{";
  json += "\"username\":\"" + user.username + "\",";
  json += "\"displayName\":\"" + user.displayName + "\",";
  json += "\"role\":\"" + user.role + "\",";
  json += "\"allowedRooms\":\"" + user.allowedRooms + "\"";
  json += "}";
  return json;
}

String AuthManager::getUsersJson() {
  String json = "[";
  for (size_t i = 0; i < _users.size(); i++) {
    if (i > 0) json += ",";
    const auto& u = _users[i];
    json += "{";
    json += "\"username\":\"" + u.username + "\",";
    json += "\"displayName\":\"" + u.displayName + "\",";
    json += "\"role\":\"" + u.role + "\",";
    json += "\"allowedRooms\":\"" + u.allowedRooms + "\"";
    json += "}";
  }
  json += "]";
  return json;
}
