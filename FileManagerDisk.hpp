#ifndef FILEMANAGERDISK_HPP
#define FILEMANAGERDISK_HPP

#include <string>
#include <vector>
#include <fstream>
#include <bitset>
#include "FileEntry.hpp"
#include "FileManager.hpp"
#include "BTree.hpp"

// Disk Configuration
const long DISK_SIZE = 2L * 1024 * 1024 * 1024;  // 2 GB
const int BLOCK_SIZE = 50 * 1024;                 // 50 KB per block
const int TOTAL_BLOCKS = DISK_SIZE / BLOCK_SIZE;  // 40,960 blocks

// Disk block metadata
struct BlockMetadata {
    int fileId;
    int blockNumber;      // Which block of the file (0, 1, 2...)
    int nextBlock;        // Next block in chain (-1 if last)
    int dataSize;         // Actual data size in this block
};

class FileManagerDisk {
private:
    std::string diskFilePath;
    std::fstream diskFile;
    
    // Free block bitmap (1 = used, 0 = free)
    std::vector<bool> blockBitmap;
    
    // B-Tree for indexing files (fileId -> first block number)
    BTree* btree;
    
    // Helper functions
    bool initializeDisk();
    int allocateBlock();
    void freeBlock(int blockNum);
    bool writeBlock(int blockNum, const BlockMetadata& meta, const char* data, int dataSize);
    bool readBlock(int blockNum, BlockMetadata& meta, char* data);
    void saveBitmap();
    void loadBitmap();
    std::vector<int> getFileBlocks(int fileId);
    
public:
    // Constructor
    FileManagerDisk(const std::string& diskPath);
    ~FileManagerDisk();
    
    // Save a file to disk (allocates blocks, stores in B-Tree)
    bool saveFile(const FileEntry& f);
    
    // Load a file from disk by fileId
    FileEntry* loadFile(int fileId);
    
    // Load all files from disk into FileManager
    bool loadAllFiles(FileManager& fm);
    
    // Delete a file from disk (frees blocks)
    bool deleteFile(int fileId);
    
    // Update an existing file on disk
    bool updateFile(const FileEntry& f);
    
    // Get disk statistics
    void printDiskStats() const;
    int getFreeBlocks() const;
    int getUsedBlocks() const;
};

#endif // FILEMANAGERDISK_HPP
