#ifndef USERMANAGER_HPP
#define USERMANAGER_HPP

#include <string>
#include <vector>
#include "User.hpp"
#include "HashMap.hpp"

class UserManagerDisk;

class UserManager {
private:
    HashMap<User> users;  
    int nextUserId;
    UserManagerDisk* disk;

public:
    UserManager();
    
    bool registerUser(const std::string& username, const std::string& password);
    
    int loginUser(const std::string& username, const std::string& password);
    
    User* getUser(const std::string& username);
    
    User* getUserById(int userId);
    
    bool userExists(const std::string& username);
    
    void loadUser(int userId, const std::string& username, const std::string& password);
    
    
    void setDiskManager(UserManagerDisk* diskMgr);
};

#endif 
