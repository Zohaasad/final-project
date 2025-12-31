#ifndef USERMANAGERDISK_HPP
#define USERMANAGERDISK_HPP

#include <string>
#include <fstream>
#include "User.hpp"
#include "UserManager.hpp"
#include <iostream>
class UserManagerDisk {
private:
    std::string diskFilePath;
    
public:
    UserManagerDisk(const std::string& path) : diskFilePath(path) {}
    
    void saveUser(const User& user) {
        std::ofstream file(diskFilePath, std::ios::app);
        if (file.is_open()) {
            file << user.userId << "|" << user.username << "|" << user.password << "\n";
            file.close();
        }
    }
    
    void loadAllUsers(UserManager& um) {
        std::ifstream file(diskFilePath);
        if (!file.is_open()) return;
        
        std::string line;
        int count = 0;
        while (std::getline(file, line)) {
            size_t pos1 = line.find('|');
            size_t pos2 = line.find('|', pos1 + 1);
            
            if (pos1 != std::string::npos && pos2 != std::string::npos) {
                int userId = std::stoi(line.substr(0, pos1));
                std::string username = line.substr(pos1 + 1, pos2 - pos1 - 1);
                std::string password = line.substr(pos2 + 1);
                
                um.loadUser(userId, username, password);
                count++;
            }
        }
        
        file.close();
        
        if (count > 0) {
            std::cout << "[System] Restored " << count << " user accounts\n";
        }
    }
};

#endif
