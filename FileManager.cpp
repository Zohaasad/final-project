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

    FileEntry* existing = searchFile(name);
    if (existing) {
        std::cerr << "[FileManager] File '" << name << "' already exists!\n";
        return false;
    }
    
    FileEntry f;
    f.fileId = nextFileId++;
    f.userId = currentUserId;
    f.name = name;
    f.content = content;
    f.createTime = std::time(nullptr);
    f.expireTime = f.createTime + expireSeconds;
    f.inBin = false;
    f.inUse = true;
    
    files.insert(f);
    
    
    if (disk) {
        disk->saveFile(f);
    }
    
    return true;
}


bool FileManager::writeFile(const std::string& fileName, const std::string& content) {
    FileEntry* f = searchFile(fileName);
    if (!f || f->inBin) return false;
    
    f->content = content;
    if (disk) {
        disk->updateFile(*f);
    }
    
    return true;
}
bool FileManager::readFile(const std::string& fileName, std::string& outContent) {
    FileEntry* f = searchFile(fileName);
    if (!f || f->inBin) return false;
    outContent = f->content;
    return true;
}
