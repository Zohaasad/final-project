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

bool FileManager::editFile(const std::string& fileName, const std::string& content) {
    return writeFile(fileName, content);
}
bool FileManager::truncateFile(const std::string& fileName) {
    FileEntry* f = searchFile(fileName);
    if (!f || f->inBin) return false;
    
    f->content = "";
    if (disk) {
        disk->updateFile(*f);
    }
    
    return true;
}

bool FileManager::moveToBin(const std::string& fileName) {
    FileEntry* f = searchFile(fileName);
    if (!f) return false;
    
    f->inBin = true;
    binFiles.insert(*f);
    files.remove(f->fileId);
    if (disk) {
        disk->updateFile(*f);
    }
    
    return true;
}
bool FileManager::moveToBinById(int fileId) {
    FileEntry* f = files.search(fileId);
    if (!f) return false;
    
    f->inBin = true;
    binFiles.insert(*f);
    
    files.remove(fileId);
    
    if (disk) {
        disk->updateFile(*f);
    }
    
    return true;
}

bool FileManager::retrieveFromBin(const std::string& fileName) {
    std::vector<FileEntry> allBin = binFiles.getAll();
    FileEntry* f = nullptr;
    
    for (auto& entry : allBin) {
        if (entry.name == fileName && entry.userId == currentUserId && entry.inBin) {
            f = binFiles.search(entry.fileId);
            break;
        }
    }
    
    if (!f) return false;
    
    long duration = f->expireTime - f->createTime;
    f->inBin = false;
    f->createTime = std::time(nullptr);
    f->expireTime = f->createTime + duration;
    
    files.insert(*f);
    binFiles.remove(f->fileId);
    if (disk) {
        disk->updateFile(*f);
    }
    
    return true;
}
bool FileManager::changeExpiry(const std::string& fileName, long newExpireSeconds) {
    FileEntry* f = searchFile(fileName);
    if (!f || f->inBin) return false;
    
    f->expireTime = std::time(nullptr) + newExpireSeconds;
    
    
    if (disk) {
        disk->updateFile(*f);
    }
    
    return true;
}
