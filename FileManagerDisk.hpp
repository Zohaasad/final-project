
#ifndef FILEMANAGERDISK_HPP
#define FILEMANAGERDISK_HPP

#include <string>
#include <vector>
#include <fstream>
#include <ctime>
#include "FileEntry.hpp"
#include "FileManager.hpp"
#include "BTree.hpp"

const long DISK_SIZE = 2L * 1024 * 1024 * 1024; 
const int BLOCK_SIZE = 50 * 1024;               
const int TOTAL_BLOCKS = DISK_SIZE / BLOCK_SIZE;

struct BlockMetadata {
    int fileId;
    int blockNumber;
    int nextBlock;
    int dataSize;
};

class FileManager;

class FileManagerDisk {
private:
    std::string diskFilePath;
    std::fstream diskFile;
    int totalBlocks;
    int usedBlocks;
    std::vector<bool> blockBitmap;
    BTree* btree;
    
    bool initializeDisk();
    void saveBitmap();
    void loadBitmap();
    int allocateBlock();
    void freeBlock(int blockNum);
    bool writeBlock(int blockNum, const BlockMetadata& meta, const char* data, int dataSize);
    bool readBlock(int blockNum, BlockMetadata& meta, char* data);
    std::vector<int> getFileBlocks(int fileId);

public:
    FileManagerDisk(const std::string& diskPath);
    ~FileManagerDisk();
    
    bool saveFile(const FileEntry& f);
    FileEntry* loadFile(int fileId);
    bool deleteFile(int fileId);
    bool updateFile(const FileEntry& f);
    bool loadAllFiles(FileManager& fm);
    
    int getUsedBlocks() const;
    int getFreeBlocks() const;
    void printDiskStats() const;
    

    std::vector<int> getAllFileIds() {
        if (btree) {
            return btree->getAllFileIds();
        }
        return std::vector<int>();
    }
};

#endif
