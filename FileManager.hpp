#ifndef FILEMANAGER_HPP
#define FILEMANAGER_HPP

#include <string>
#include <vector>
#include "HashMap.hpp"
#include "FileEntry.hpp"

class FileManagerDisk;

class FileManager {
private:
   private:
    HashMap<FileEntry> files;       
    HashMap<FileEntry> binFiles;     
    int nextFileId;
    FileManagerDisk* disk;
    int currentUserId;   

public:
   
    FileManager(int initialSize);
    void setCurrentUser(int userId);
    
   
    int getCurrentUser() const;
    void setDiskManager(FileManagerDisk* diskMgr);
    bool createFile(const std::string& name, const std::string& content, long expireSeconds);
    bool writeFile(const std::string& fileName, const std::string& content);
    bool editFile(const std::string& fileName, const std::string& content);
    
   
    bool readFile(const std::string& fileName, std::string& outContent);
    
    
    bool truncateFile(const std::string& fileName);
    bool moveToBin(const std::string& fileName);
    bool moveToBinById(int fileId); 
    bool retrieveFromBin(const std::string& fileName);
    bool changeExpiry(const std::string& fileName, long newExpireSeconds);
    FileEntry* searchFile(const std::string& fileName);
    FileEntry* searchFileById(int fileId);
    void listFiles() const;
    std::vector<FileEntry> getActiveFiles() const;
    std::vector<FileEntry> getBinFiles() const;
    bool removeFileCompletely(const std::string& fileName);

    void loadFileEntry(const FileEntry& f);
};

#endif 
