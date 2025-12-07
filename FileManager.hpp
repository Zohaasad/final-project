#ifndef FILEMANAGER_HPP
#define FILEMANAGER_HPP

#include <string>
#include "HashMap.hpp"

class FileManager {
private:
    HashMap files;        // active memory
    HashMap binFiles;     // recycle bin
    int nextFileId;

public:
    // Constructor
    FileManager(int initialSize);

    // Create a file
    int createFile(const std::string& name, const std::string& content, long expireSeconds);

    // Write / edit
    bool writeFile(int fileId, const std::string& content);
    bool editFile(int fileId, const std::string& content);

    // Read
    bool readFile(int fileId, std::string& outContent);

    // Truncate
    bool truncateFile(int fileId);

    // Bin operations
    bool moveToBin(int fileId);
    bool retrieveFromBin(int fileId);

    // Change timer
    bool changeExpiry(int fileId, long newExpireSeconds);

    // Search in both memories
    FileEntry* searchFile(int fileId);

    // Display all
    void listFiles() const;
     std::vector<FileEntry> getActiveFiles() const;
    
    // Get all bin files
    std::vector<FileEntry> getBinFiles() const;
    
    // Remove file completely from memory
    bool removeFileCompletely(int fileId);
    void loadFileEntry(const FileEntry& f);
};

#endif
