#include "UserManager.hpp"
#include "UserManagerDisk.hpp"
#include <iostream>

UserManager::UserManager() : users(10), nextUserId(1), disk(nullptr) {}

bool UserManager::registerUser(const std::string& username, const std::string& password) {
    if (userExists(username)) {
        return false;
    }
    
    User newUser;
    newUser.userId = nextUserId++;
    newUser.username = username;
    newUser.password = password;
    newUser.inUse = true;
    
    users.insert(newUser);
    
    if (disk) {
        disk->saveUser(newUser);
    }
    
    std::cout << "[UserManager] User '" << username << "' registered successfully (ID: " << newUser.userId << ")\n";
    return true;
}

int UserManager::loginUser(const std::string& username, const std::string& password) {
    if (!userExists(username)) {
        return -1;
    }
    
    User* user = users.search(username);
    if (user && user->password == password) {
        std::cout << "[UserManager] User '" << username << "' logged in successfully\n";
        return user->userId;
    }
    
    return -1;
}

User* UserManager::getUser(const std::string& username) {
    return users.search(username);
}

User* UserManager::getUserById(int userId) {
    return users.search(userId);
}

bool UserManager::userExists(const std::string& username) {
    return users.search(username) != nullptr;
}

void UserManager::setDiskManager(UserManagerDisk* diskMgr) {
    disk = diskMgr;
}

void UserManager::loadUser(int userId, const std::string& username, const std::string& password) {
    User newUser;
    newUser.userId = userId;
    newUser.username = username;
    newUser.password = password;
    newUser.inUse = true;
    users.insert(newUser);
    
    if (userId >= nextUserId) {
        nextUserId = userId + 1;
    }
}
