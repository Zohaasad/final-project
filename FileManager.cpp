#include "FileManager.hpp"
#include <iostream>
#include <ctime>

// Constructor
FileManager::FileManager(int initialSize)
    : files(initialSize), binFiles(initialSize), nextFileId(1) {}


// Create a new file
int FileManager::createFile(const std::string& name, const std::string& content, long expireSeconds) {
    FileEntry f;
    f.fileId = nextFileId++;
    f.name = name;
    f.content = content;
    f.createTime = std::time(nullptr);
    f.expireTime = f.createTime + expireSeconds;
    f.inBin = false;
    f.inUse = true;

    files.insert(f);
    return f.fileId;
}


// Write content
bool FileManager::writeFile(int fileId, const std::string& content) {
    FileEntry* f = files.search(fileId);
    if (!f || f->inBin) return false;

    f->content = content;
    return true;
}


// Read content
bool FileManager::readFile(int fileId, std::string& outContent) {
    FileEntry* f = files.search(fileId);
    if (!f || f->inBin) return false;

    outContent = f->content;
    return true;
}


// Edit (same as write)
bool FileManager::editFile(int fileId, const std::string& content) {
    return writeFile(fileId, content);
}


// Truncate (clear content)
bool FileManager::truncateFile(int fileId) {
    FileEntry* f = files.search(fileId);
    if (!f || f->inBin) return false;

    f->content = "";
    return true;
}


// Move file to bin
bool FileManager::moveToBin(int fileId) {
    FileEntry* f = files.search(fileId);
    if (!f) return false;

    f->inBin = true;

    // insert into bin
    binFiles.insert(*f);

    // remove from active
    files.remove(fileId);

    return true;
}


// Restore from bin (restart expiry)
bool FileManager::retrieveFromBin(int fileId) {
    FileEntry* f = binFiles.search(fileId);
    if (!f) return false;

    long duration = f->expireTime - f->createTime;

    f->inBin = false;
    f->createTime = std::time(nullptr);
    f->expireTime = f->createTime + duration;

    files.insert(*f);
    binFiles.remove(fileId);

    return true;
}


// Change file expiry
bool FileManager::changeExpiry(int fileId, long newExpireSeconds) {
    FileEntry* f = files.search(fileId);
    if (!f || f->inBin) return false;

    f->expireTime = std::time(nullptr) + newExpireSeconds;
    return true;
}


// Search in active first, then in bin
FileEntry* FileManager::searchFile(int fileId) {
    FileEntry* f = files.search(fileId);
    if (f) return f;

    return binFiles.search(fileId);
}


// List all active + deleted files
void FileManager::listFiles() const {
    std::cout << "Active Files:\n";
    for (auto file : files.getAll()) {
        if (!file.inBin) {
            std::cout << "ID: " << file.fileId
                      << ", Name: " << file.name
                      << ", Content: " << file.content
                      << ", Created: " << file.createTime
                      << ", Expires: " << file.expireTime
                      << "\n";
        }
    }

    std::cout << "\nFiles in Bin:\n";
    for (auto file : binFiles.getAll()) {
        if (file.inBin) {
            std::cout << "ID: " << file.fileId
                      << ", Name: " << file.name
                      << ", Created: " << file.createTime
                      << ", Expires: " << file.expireTime
                      << "\n";
        }
    }
}
// ... (all your existing code)

// Get all active files
std::vector<FileEntry> FileManager::getActiveFiles() const {
    return files.getAll();
}

// Get all bin files
std::vector<FileEntry> FileManager::getBinFiles() const {
    return binFiles.getAll();
}

// Remove file completely from memory
bool FileManager::removeFileCompletely(int fileId) {
    FileEntry* f = searchFile(fileId);
    if (!f) return false;
    
    if (f->inBin) {
        return binFiles.remove(fileId);
    } else {
        return files.remove(fileId);
    }
}
// Load a file entry (used by disk on startup)
void FileManager::loadFileEntry(const FileEntry& f) {
    if (f.inBin) {
        binFiles.insert(f);
    } else {
        files.insert(f);
    }
    
    // Update nextFileId to avoid conflicts
    if (f.fileId >= nextFileId) {
        nextFileId = f.fileId + 1;
    }
}
