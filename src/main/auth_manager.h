#ifndef SMARTHOME_AUTH_MANAGER_H
#define SMARTHOME_AUTH_MANAGER_H

#include <Arduino.h>
#include <vector>
#include "config.h"

struct User {
  String username;
  String password;
  String displayName;
  String role;         // "admin" or "user"
  String allowedRooms; // comma-separated: "bedroom_1,living_room" or "*"

  bool isAdmin() const {
    return role.equalsIgnoreCase("admin");
  }
};

struct Session {
  String token;
  String username;
  unsigned long createdAt;
};

class AuthManager {
public:
  static AuthManager& getInstance();

  void begin();

  // Authentication & Session
  bool login(const String& username, const String& password, String& outToken, User& outUser);
  void logout(const String& token);
  bool validateToken(const String& token, User& outUser);

  // Permission Checks
  bool canAccessRoom(const User& user, const String& roomId);
  bool canAccessDevice(const User& user, const String& roomId, const String& deviceId);

  // Admin User Management
  std::vector<User> getAllUsers();
  bool addUser(const User& newUser, String& errorMsg);
  bool updateUser(const User& updatedUser, String& errorMsg);
  bool deleteUser(const String& username, String& errorMsg);

  String getUsersJson();
  String getUserProfileJson(const User& user);

private:
  AuthManager();
  ~AuthManager() = default;

  void loadUsersFromNvs();
  void saveUsersToNvs();
  void initDefaultUsers();
  String generateToken();
  void cleanExpiredSessions();

  std::vector<User> _users;
  std::vector<Session> _sessions;
};

#endif // SMARTHOME_AUTH_MANAGER_H
