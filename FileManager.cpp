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


FileEntry* FileManager::searchFile(const std::string& fileName) {
    if (currentUserId == -1) return nullptr;
    std::vector<FileEntry> allFiles = files.getAll();
    for (auto& f : allFiles) {
        if (f.name == fileName && f.userId == currentUserId && !f.inBin) {
            return files.search(f.fileId);
        }
    }
    

    std::vector<FileEntry> allBin = binFiles.getAll();
    for (auto& f : allBin) {
        if (f.name == fileName && f.userId == currentUserId && f.inBin) {
            return binFiles.search(f.fileId);
        }
    }
    
    return nullptr;
}

FileEntry* FileManager::searchFileById(int fileId) {
    FileEntry* f = files.search(fileId);
    if (f) return f;
    return binFiles.search(fileId);
}


void FileManager::listFiles() const {
    if (currentUserId == -1) {
        std::cout << "No user logged in!\n";
        return;
    }
    
    std::cout << "\n========== YOUR ACTIVE FILES ==========\n";
    bool hasActive = false;
    for (auto file : files.getAll()) {
        if (file.userId == currentUserId && !file.inBin) {
            hasActive = true;
            std::cout << "File: " << file.name
                      << "\n  Content: " << file.content
                      << "\n  Created: " << ctime(&file.createTime)
                      << "  Expires: " << ctime(&file.expireTime);
        }
    }
    if (!hasActive) {
        std::cout << "No active files.\n";
    }
    
    std::cout << "\n========== YOUR FILES IN BIN ==========\n";
    bool hasBin = false;
    for (auto file : binFiles.getAll()) {
        if (file.userId == currentUserId && file.inBin) {
            hasBin = true;
            std::cout << "File: " << file.name
                      << "\n  Created: " << ctime(&file.createTime)
                      << "  Expires: " << ctime(&file.expireTime);
        }
    }
    if (!hasBin) {
        std::cout << "No files in bin.\n";
    }
    std::cout << "=======================================\n\n";
}

std::vector<FileEntry> FileManager::getActiveFiles() const {
    std::vector<FileEntry> userFiles;
    for (auto f : files.getAll()) {
        if (f.userId == currentUserId && !f.inBin) {
            userFiles.push_back(f);
        }
    }
    return userFiles;
}

std::vector<FileEntry> FileManager::getBinFiles() const {
    std::vector<FileEntry> userFiles;
    for (auto f : binFiles.getAll()) {
        if (f.userId == currentUserId && f.inBin) {
            userFiles.push_back(f);
        }
    }
    return userFiles;
}


bool FileManager::removeFileCompletely(const std::string& fileName) {
    FileEntry* f = searchFile(fileName);
    if (!f) return false;
    
   
    if (disk) {
        disk->deleteFile(f->fileId);
    }
    
    if (f->inBin) {
        return binFiles.remove(f->fileId);
    } else {
        return files.remove(f->fileId);
    }
}


void FileManager::loadFileEntry(const FileEntry& f) {
    if (f.inBin) {
        binFiles.insert(f);
    } else {
        files.insert(f);
    }
    
  
    if (f.fileId >= nextFileId) {
        nextFileId = f.fileId + 1;
    }
}


