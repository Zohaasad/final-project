
#ifndef FILEMANAGER_HPP
#define FILEMANAGER_HPP

#include <string>
#include <vector>
#include <ctime>
#include <iostream>
#include "FileEntry.hpp"
#include "FileManagerDisk.hpp"
#include "MinHeap.hpp"
#include "HashMap.hpp"

class FileManagerDisk;

class FileManager {
private:
    HashMap<FileEntry> fileMap;    
    int currentUserId;
    FileManagerDisk* diskManager;
    FileEntryHeap expiryHeap;        

public:
    FileManager(int userId = -1);
    ~FileManager() = default;

    void setCurrentUser(int userId);
    void setDiskManager(FileManagerDisk* dm);

    bool loadUserFiles(int userId);   
    void unloadUserFiles(int userId);  

    bool createFile(const std::string& name, const std::string& content, long expireSeconds);
    bool writeFile(const std::string& name, const std::string& content);
    bool readFile(const std::string& name, std::string& content);
    bool truncateFile(const std::string& name);
    bool moveToBin(const std::string& name);
    bool retrieveFromBin(const std::string& name);
    bool changeExpiry(const std::string& name, long newExpireSeconds);
    bool removeFileCompletely(const std::string& name);

    FileEntry* searchFile(const std::string& name);
    FileEntry* searchFileById(int fileId);
    void updateExpiryStatus();
    std::vector<FileEntry> getActiveFiles() const;
    std::vector<FileEntry> getBinFiles() const;
    bool restoreFile(int fileId, const std::string& name, const std::string& content, 
                     long expireSeconds, int ownerId, bool wasInBin, 
                     time_t originalCreateTime = 0, time_t originalExpireTime = 0);

    void displayAllFiles() const;
};

#endif
