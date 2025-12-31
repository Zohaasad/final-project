#ifndef USER_HPP
#define USER_HPP

#include <string>

struct User {
    int userId;
    std::string username;
    std::string password;
    bool inUse = false; 
};

#endif 
