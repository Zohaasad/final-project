
#include "FileManager.hpp"
#include <algorithm>
#include <iostream>
#include <ctime>

FileManager::FileManager(int userId)
    : currentUserId(userId), diskManager(nullptr), expiryHeap(), fileMap(100) {}

void FileManager::setCurrentUser(int userId) {
    currentUserId = userId;
}

void FileManager::setDiskManager(FileManagerDisk* dm) {
    diskManager = dm;
}

bool FileManager::loadUserFiles(int userId) {
    if (!diskManager) {
        std::cerr << "[FileManager] No disk manager set!\n";
        return false;
    }
    
    std::cout << "[FileManager] Loading files for user " << userId << "...\n";
    std::vector<int> allFileIds = diskManager->getAllFileIds();
    
    time_t now = std::time(nullptr);
    int loadedCount = 0;
    int activeCount = 0;
    int binCount = 0;
    
    for (int fileId : allFileIds) {
 
        FileEntry* diskFile = diskManager->loadFile(fileId);
        if (!diskFile) continue;
        if (diskFile->userId != userId) {
            delete diskFile;
            continue;
        }
        if (fileMap.search(fileId) != nullptr) {
            std::cout << "[FileManager] File " << fileId << " already in memory, skipping\n";
            delete diskFile;
            continue;
        }
        if (diskFile->expireTime < now) {
            diskFile->inBin = true;
            diskFile->inUse = true;
            diskManager->updateFile(*diskFile);
            binCount++;
            std::cout << "[FileManager] File '" << diskFile->name << "' expired, loaded to BIN\n";
        } else {
          
            diskFile->inBin = false;
            diskFile->inUse = true;
            activeCount++;
            std::cout << "[FileManager] File '" << diskFile->name << "' loaded as ACTIVE\n";
        }
        
     
        if (fileMap.insert(*diskFile)) {
  
            if (!diskFile->inBin) {
                FileEntry* filePtr = fileMap.search(fileId);
                if (filePtr) {
                    expiryHeap.push(filePtr);
                }
            }
        }
        
        delete diskFile;
        loadedCount++;
    }
    
    std::cout << "[FileManager] Loaded " << loadedCount << " files for user " << userId 
              << " (" << activeCount << " active, " << binCount << " in bin)\n";
    
    return true;
}

void FileManager::unloadUserFiles(int userId) {
    std::cout << "[FileManager] Unloading files for user " << userId << "...\n";
    
    std::vector<FileEntry> allFiles = fileMap.getAll();
    int removedCount = 0;
    
    for (auto& file : allFiles) {
        if (file.userId == userId) {
         
            if (!file.inBin) {
                FileEntry* filePtr = fileMap.search(file.fileId);
                if (filePtr) {
                    expiryHeap.remove(filePtr);
                }
            }
            
          
            fileMap.remove(file.fileId);
            removedCount++;
            
            std::cout << "[FileManager] Removed file '" << file.name << "' from memory\n";
        }
    }
    
    std::cout << "[FileManager] Unloaded " << removedCount << " files for user " << userId << "\n";
}

bool FileManager::createFile(const std::string& name, const std::string& content, long expireSeconds) {
    if (name.empty()) {
        std::cerr << "[FileManager] Cannot create file with empty name.\n";
        return false;
    }

    std::vector<FileEntry> allFiles = fileMap.getAll();
    for (auto& f : allFiles) {
        if (f.userId == currentUserId && f.name == name && !f.inBin) {
            std::cerr << "[FileManager] File with name '" << name << "' already exists.\n";
            return false;
        }
    }

    int maxId = 0;
    if (diskManager) {
        std::vector<int> allFileIds = diskManager->getAllFileIds();
        for (int id : allFileIds) {
            if (id > maxId) maxId = id;
        }
    }
    
    for (auto& existing : allFiles) {
        if (existing.fileId > maxId) maxId = existing.fileId;
    }

    FileEntry f;
    f.fileId = maxId + 1;
    f.userId = currentUserId;
    f.ownerId = currentUserId;
    f.name = name;
    f.content = content;
    f.createTime = std::time(nullptr);
    f.expireTime = f.createTime + expireSeconds;
    f.inBin = false;
    f.inUse = true;
    f.expired = false;

    if (diskManager) {
        if (!diskManager->saveFile(f)) {
            std::cerr << "[FileManager] CRITICAL: Failed to save file to disk!\n";
            return false;
        }
        std::cout << "[FileManager] File '" << name << "' saved to disk (ID: " << f.fileId << ")\n";
    }
    if (!fileMap.insert(f)) {
        std::cerr << "[FileManager] Failed to insert file into HashMap.\n";
        return false;
    }
    FileEntry* filePtr = fileMap.search(f.fileId);
    if (filePtr) {
        expiryHeap.push(filePtr);
    }

    std::cout << "[FileManager] File '" << name << "' created in memory (ID: " << f.fileId << ")\n";

    return true;
}

bool FileManager::writeFile(const std::string& name, const std::string& content) {
    FileEntry* f = searchFile(name);
    if (!f) {
        std::cerr << "[FileManager] File '" << name << "' not found.\n";
        return false;
    }
    
    if (f->inBin) {
        std::cerr << "[FileManager] Cannot write to file in bin.\n";
        return false;
    }

    f->content = content;
    
    if (diskManager) {
        if (!diskManager->saveFile(*f)) {
            std::cerr << "[FileManager] CRITICAL: Failed to update file on disk!\n";
            return false;
        }
        std::cout << "[FileManager] File '" << name << "' updated on disk.\n";
    }
    
    return true;
}

bool FileManager::readFile(const std::string& name, std::string& content) {
    FileEntry* f = searchFile(name);
    if (!f) return false;
    if (f->inBin) return false;

    content = f->content;
    return true;
}

bool FileManager::truncateFile(const std::string& name) {
    FileEntry* f = searchFile(name);
    if (!f) return false;
    if (f->inBin) return false;

    f->content.clear();
    
    if (diskManager) {
        if (!diskManager->saveFile(*f)) {
            std::cerr << "[FileManager] CRITICAL: Failed to truncate file on disk!\n";
            return false;
        }
        std::cout << "[FileManager] File '" << name << "' truncated on disk.\n";
    }
    
    return true;
}

bool FileManager::moveToBin(const std::string& name) {
    FileEntry* f = searchFile(name);
    if (!f) return false;
    if (f->inBin) return false;

    expiryHeap.remove(f);

    f->inBin = true;
    f->inUse = true;

    if (diskManager) {
        if (!diskManager->updateFile(*f)) {
            std::cerr << "[FileManager] CRITICAL: Failed to update file status on disk!\n";
            return false;
        }
    }
    
    std::cout << "[FileManager] File '" << name << "' moved to bin.\n";
    return true;
}

bool FileManager::retrieveFromBin(const std::string& name) {
    FileEntry* f = searchFile(name);
    if (!f) {
        std::cerr << "[FileManager] File '" << name << "' not found.\n";
        return false;
    }
    
    if (!f->inBin) {
        std::cerr << "[FileManager] File '" << name << "' is not in bin.\n";
        return false;
    }

    f->inBin = false;
    f->inUse = true;

    long remainingTime = f->expireTime - std::time(nullptr);
    if (remainingTime <= 0) {
        f->expireTime = std::time(nullptr) + 3600; 
        std::cout << "[FileManager] File was expired. Expiry reset to 1 hour from now.\n";
    }

    expiryHeap.push(f);

    if (diskManager) {
        if (!diskManager->updateFile(*f)) {
            std::cerr << "[FileManager] CRITICAL: Failed to update file status on disk!\n";
            return false;
        }
    }
    
    std::cout << "[FileManager] File '" << name << "' retrieved from bin.\n";
    return true;
}

void FileManager::updateExpiryStatus() {
    time_t now = std::time(nullptr);
    
    while (!expiryHeap.isEmpty()) {
        FileEntry* f = expiryHeap.peek();
        if (!f) break;
        
        if (f->expireTime > now) break;

        expiryHeap.extractMin();
        
        f->inBin = true;
        f->inUse = true;

        if (diskManager) {
            diskManager->updateFile(*f);
        }
        
        std::cout << "[AUTO-EXPIRY] File '" << f->name << "' (ID: " << f->fileId << ") expired and moved to bin.\n";
    }
}

FileEntry* FileManager::searchFile(const std::string& name) {
   
    std::vector<FileEntry> allFiles = fileMap.getAll();
    for (auto& f : allFiles) {
        if (f.userId == currentUserId && f.name == name) {
            return fileMap.search(f.fileId);
        }
    }
    return nullptr;
}

FileEntry* FileManager::searchFileById(int fileId) {
    // Direct lookup by ID in HashMap
    FileEntry* f = fileMap.search(fileId);
    if (f && f->inUse) {
        return f;
    }
    return nullptr;
}

bool FileManager::restoreFile(int fileId, const std::string& name, const std::string& content, 
                              long expireSeconds, int ownerId, bool wasInBin, 
                              time_t originalCreateTime, time_t originalExpireTime) {
    std::cerr << "[FileManager] restoreFile() is deprecated. Use loadUserFiles() instead.\n";
    return false;
}

std::vector<FileEntry> FileManager::getActiveFiles() const {
    std::vector<FileEntry> active;
    std::vector<FileEntry> allFiles = fileMap.getAll();
    
    for (const auto& f : allFiles) {
        if (f.userId == currentUserId && !f.inBin && f.inUse) {
            active.push_back(f);
        }
    }
    return active;
}

std::vector<FileEntry> FileManager::getBinFiles() const {
    std::vector<FileEntry> binFiles;
    std::vector<FileEntry> allFiles = fileMap.getAll();
    
    for (const auto& f : allFiles) {
        if (f.userId == currentUserId && f.inBin && f.inUse) {
            binFiles.push_back(f);
        }
    }
    return binFiles;
}

void FileManager::displayAllFiles() const {
    std::cout << "\n========== Active Files ==========\n";
    auto activeFiles = getActiveFiles();
    if (activeFiles.empty()) {
        std::cout << "No active files.\n";
    } else {
        for (const auto& f : activeFiles) {
            std::cout << "ID: " << f.fileId 
                      << " | Name: " << f.name 
                      << " | Size: " << f.content.size() << " bytes"
                      << " | Expires: " << f.expireTime << "\n";
        }
    }

    std::cout << "\n========== Files in Bin ==========\n";
    auto binFiles = getBinFiles();
    if (binFiles.empty()) {
        std::cout << "No files in bin.\n";
    } else {
        for (const auto& f : binFiles) {
            std::cout << "ID: " << f.fileId 
                      << " | Name: " << f.name 
                      << " | Size: " << f.content.size() << " bytes\n";
        }
    }
    std::cout << "==================================\n\n";
}

bool FileManager::changeExpiry(const std::string& name, long newExpireSeconds) {
    FileEntry* f = searchFile(name);
    if (!f) return false;
    if (f->inBin) return false;

    f->expireTime = std::time(nullptr) + newExpireSeconds;
    

    expiryHeap.update(f);
    
    if (diskManager) {
        diskManager->updateFile(*f);
    }
    
    return true;
}

bool FileManager::removeFileCompletely(const std::string& name) {
    FileEntry* f = searchFile(name);
    if (!f) return false;

    int fileId = f->fileId;
    bool wasInBin = f->inBin;

    if (diskManager) {
        if (!diskManager->deleteFile(fileId)) {
            std::cerr << "[FileManager] Failed to delete file from disk!\n";
            return false;
        }
    }
    if (!wasInBin) {
        expiryHeap.remove(f);
    }
    if (!fileMap.remove(fileId)) {
        std::cerr << "[FileManager] Failed to remove file from HashMap!\n";
        return false;
    }

    std::cout << "[FileManager] File '" << name << "' permanently deleted.\n";
    return true;
}
