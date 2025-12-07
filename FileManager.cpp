#include "FileManager.hpp"
#include "FileManagerDisk.hpp"
#include <iostream>
#include <ctime>
FileManager::FileManager(int initialSize)
    : files(initialSize), binFiles(initialSize), nextFileId(1), disk(nullptr), currentUserId(-1) {}


void FileManager::setCurrentUser(int userId) {
    currentUserId = userId;
}

int FileManager::getCurrentUser() const {
    return currentUserId;
}


void FileManager::setDiskManager(FileManagerDisk* diskMgr) {
    disk = diskMgr;
}


bool FileManager::createFile(const std::string& name, const std::string& content, long expireSeconds) {
    if (currentUserId == -1) {
        std::cerr << "[FileManager] No user logged in!\n";
        return false;
    }
    
