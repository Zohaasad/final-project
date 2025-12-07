#ifndef FILEENTRY_HPP
#define FILEENTRY_HPP

#include <string>
#include <ctime>

struct FileEntry {
    int fileId;          
    int userId;          
    std::string name;   
    std::string content;
    time_t createTime;
    time_t expireTime;
    bool inBin = false;  
    bool inUse = false;  
};

#endif 
