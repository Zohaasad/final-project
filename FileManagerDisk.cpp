#include "FileManagerDisk.hpp"
#include <iostream>
#include <cstring>
#include <ctime>
#include <sstream>
#include <algorithm>

// Constructor
FileManagerDisk::FileManagerDisk(const std::string& diskPath) : diskFilePath(diskPath) {
    blockBitmap.resize(TOTAL_BLOCKS, false); // false = free
    btree = new BTree(3, diskPath + "_btree.dat");
    
    // Open or create disk file
    diskFile.open(diskFilePath, std::ios::in | std::ios::out | std::ios::binary);
    
    if (!diskFile.is_open()) {
        // Create new disk
        std::cout << "[Disk] Creating new disk: " << diskFilePath << "\n";
        std::cout << "[Disk] Size: 2GB, Block Size: 50KB, Total Blocks: " << TOTAL_BLOCKS << "\n";
        initializeDisk();
    } else {
        // Load existing disk
        std::cout << "[Disk] Loading existing disk: " << diskFilePath << "\n";
        loadBitmap();
    }
}

FileManagerDisk::~FileManagerDisk() {
    if (diskFile.is_open()) {
        saveBitmap();
        diskFile.close();
    }
    delete btree;
}

// Initialize a new disk file
bool FileManagerDisk::initializeDisk() {
    diskFile.open(diskFilePath, std::ios::out | std::ios::binary);
    if (!diskFile.is_open()) {
        std::cerr << "[Disk Error] Could not create disk file\n";
        return false;
    }
    
    // Reserve space for bitmap at the beginning (first block)
    char emptyBlock[BLOCK_SIZE] = {0};
    diskFile.write(emptyBlock, BLOCK_SIZE);
    blockBitmap[0] = true; // Block 0 is reserved for bitmap
    
    diskFile.close();
    diskFile.open(diskFilePath, std::ios::in | std::ios::out | std::ios::binary);
    saveBitmap();
    
    std::cout << "[Disk] Disk initialized successfully\n";
    return true;
}

// Allocate a free block
int FileManagerDisk::allocateBlock() {
    for (int i = 1; i < TOTAL_BLOCKS; i++) { // Start from 1 (0 is bitmap)
        if (!blockBitmap[i]) {
            blockBitmap[i] = true;
            return i;
        }
    }
    std::cerr << "[Disk Error] No free blocks available!\n";
    return -1;
}

// Free a block
void FileManagerDisk::freeBlock(int blockNum) {
    if (blockNum > 0 && blockNum < TOTAL_BLOCKS) {
        blockBitmap[blockNum] = false;
    }
}

// Write data to a block
bool FileManagerDisk::writeBlock(int blockNum, const BlockMetadata& meta, const char* data, int dataSize) {
    if (blockNum < 0 || blockNum >= TOTAL_BLOCKS) return false;
    
    long offset = (long)blockNum * BLOCK_SIZE;
    diskFile.seekp(offset);
    
    // Write metadata
    diskFile.write(reinterpret_cast<const char*>(&meta), sizeof(BlockMetadata));
    
    // Write data
    diskFile.write(data, dataSize);
    
    // Pad rest of block with zeros
    int padding = BLOCK_SIZE - sizeof(BlockMetadata) - dataSize;
    if (padding > 0) {
        char* zeros = new char[padding];
        memset(zeros, 0, padding);
        diskFile.write(zeros, padding);
        delete[] zeros;
    }
    
    diskFile.flush();
    return true;
}

// Read data from a block
bool FileManagerDisk::readBlock(int blockNum, BlockMetadata& meta, char* data) {
    if (blockNum < 0 || blockNum >= TOTAL_BLOCKS) return false;
    
    long offset = (long)blockNum * BLOCK_SIZE;
    diskFile.seekg(offset);
    
    // Read metadata
    diskFile.read(reinterpret_cast<char*>(&meta), sizeof(BlockMetadata));
    
    // Read data
    diskFile.read(data, BLOCK_SIZE - sizeof(BlockMetadata));
    
    return true;
}

// Save bitmap to block 0
void FileManagerDisk::saveBitmap() {
    diskFile.seekp(0);
    
    // Convert vector<bool> to bytes
    int numBytes = (TOTAL_BLOCKS + 7) / 8;
    char* bitmapBytes = new char[numBytes];
    memset(bitmapBytes, 0, numBytes);
    
    for (int i = 0; i < TOTAL_BLOCKS; i++) {
        if (blockBitmap[i]) {
            bitmapBytes[i / 8] |= (1 << (i % 8));
        }
    }
    
    diskFile.write(bitmapBytes, numBytes);
    diskFile.flush();
    delete[] bitmapBytes;
}

// Load bitmap from block 0
void FileManagerDisk::loadBitmap() {
    diskFile.seekg(0);
    
    int numBytes = (TOTAL_BLOCKS + 7) / 8;
    char* bitmapBytes = new char[numBytes];
    diskFile.read(bitmapBytes, numBytes);
    
    for (int i = 0; i < TOTAL_BLOCKS; i++) {
        blockBitmap[i] = (bitmapBytes[i / 8] & (1 << (i % 8))) != 0;
    }
    
    delete[] bitmapBytes;
    std::cout << "[Disk] Bitmap loaded. Used blocks: " << getUsedBlocks() << "/" << TOTAL_BLOCKS << "\n";
}

// Get all blocks belonging to a file
std::vector<int> FileManagerDisk::getFileBlocks(int fileId) {
    std::vector<int> blocks;
    
    // Search B-Tree for first block
    FileEntry* result = btree->search(fileId);
    
    if (!result) return blocks;
    
    // The B-Tree stores fileId, we need to store firstBlock differently
    // For now, scan blocks to find file (inefficient but works)
    for (int i = 1; i < TOTAL_BLOCKS; i++) {
        if (!blockBitmap[i]) continue;
        
        BlockMetadata meta;
        char data[BLOCK_SIZE];
        if (readBlock(i, meta, data)) {
            if (meta.fileId == fileId && meta.blockNumber == 0) {
                // Found first block
                int currentBlock = i;
                while (currentBlock != -1) {
                    blocks.push_back(currentBlock);
                    BlockMetadata m;
                    char d[BLOCK_SIZE];
                    readBlock(currentBlock, m, d);
                    currentBlock = m.nextBlock;
                }
                break;
            }
        }
    }
    
    return blocks;
}

// Save file to disk
bool FileManagerDisk::saveFile(const FileEntry& f) {
    try {
        std::cout << "[Disk] Auto-saving file ID " << f.fileId << "...\n";
        
        // Check if file already exists, if so delete it first
        FileEntry* existing = btree->search(f.fileId);
        if (existing) {
            deleteFile(f.fileId);
        }
        
        // Serialize FileEntry to string
        std::string serialized;
        serialized += std::to_string(f.fileId) + "\n";
        serialized += f.name + "\n";
        serialized += f.content + "\n";
        serialized += std::to_string(f.createTime) + "\n";
        serialized += std::to_string(f.expireTime) + "\n";
        serialized += std::to_string(f.inBin) + "\n";
        
        const char* dataToWrite = serialized.c_str();
        int totalSize = serialized.size();
        int dataPerBlock = BLOCK_SIZE - sizeof(BlockMetadata);
        
        // Calculate blocks needed
        int blocksNeeded = (totalSize + dataPerBlock - 1) / dataPerBlock;
        
        // Allocate blocks
        std::vector<int> allocatedBlocks;
        for (int i = 0; i < blocksNeeded; i++) {
            int block = allocateBlock();
            if (block == -1) {
                std::cerr << "[Disk Error] Failed to allocate blocks\n";
                for (int b : allocatedBlocks) freeBlock(b);
                return false;
            }
            allocatedBlocks.push_back(block);
        }
        
        // Write data to blocks
        int offset = 0;
        for (int i = 0; i < blocksNeeded; i++) {
            BlockMetadata meta;
            meta.fileId = f.fileId;
            meta.blockNumber = i;
            meta.nextBlock = (i < blocksNeeded - 1) ? allocatedBlocks[i + 1] : -1;
            
            int sizeToWrite = std::min(dataPerBlock, totalSize - offset);
            meta.dataSize = sizeToWrite;
            
            writeBlock(allocatedBlocks[i], meta, dataToWrite + offset, sizeToWrite);
            offset += sizeToWrite;
        }
        
        // Store in B-Tree (map fileId to itself for indexing)
        FileEntry btreeEntry;
btreeEntry.fileId = f.fileId;
btreeEntry.name = f.name;
btreeEntry.content = "";
btreeEntry.createTime = f.createTime;
btreeEntry.expireTime = f.expireTime;
btreeEntry.inBin = f.inBin;
btreeEntry.inUse = true;
        btreeEntry.name[49] = '\0';
        btree->insert(btreeEntry);
        
        saveBitmap();
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[Disk Error] Failed to save file: " << e.what() << "\n";
        return false;
    }
}

// Load file from disk
FileEntry* FileManagerDisk::loadFile(int fileId) {
    try {
        // Find first block by scanning
        int firstBlock = -1;
        for (int i = 1; i < TOTAL_BLOCKS; i++) {
            if (!blockBitmap[i]) continue;
            
            BlockMetadata meta;
            char data[BLOCK_SIZE];
            if (readBlock(i, meta, data)) {
                if (meta.fileId == fileId && meta.blockNumber == 0) {
                    firstBlock = i;
                    break;
                }
            }
        }
        
        if (firstBlock == -1) {
            return nullptr;
        }
        
        // Read all blocks
        std::string reconstructed;
        int currentBlock = firstBlock;
        
        while (currentBlock != -1) {
            BlockMetadata meta;
            char data[BLOCK_SIZE];
            
            if (!readBlock(currentBlock, meta, data)) {
                std::cerr << "[Disk Error] Failed to read block " << currentBlock << "\n";
                return nullptr;
            }
            
            reconstructed.append(data, meta.dataSize);
            currentBlock = meta.nextBlock;
        }
        
        // Deserialize
        FileEntry* f = new FileEntry();
        std::istringstream iss(reconstructed);
        std::string line;
        
        std::getline(iss, line); f->fileId = std::stoi(line);
        std::getline(iss, f->name);
        std::getline(iss, f->content);
        std::getline(iss, line); f->createTime = std::stol(line);
        std::getline(iss, line); f->expireTime = std::stol(line);
        std::getline(iss, line); f->inBin = std::stoi(line);
        f->inUse = true;
        
        return f;
        
    } catch (const std::exception& e) {
        std::cerr << "[Disk Error] Failed to load file: " << e.what() << "\n";
        return nullptr;
    }
}

// Load all files from disk
bool FileManagerDisk::loadAllFiles(FileManager& fm) {
    std::cout << "[Disk] Scanning disk for files...\n";
    
    int filesLoaded = 0;
    
    // Scan all blocks for first blocks (blockNumber == 0)
    for (int i = 1; i < TOTAL_BLOCKS; i++) {
        if (!blockBitmap[i]) continue;
        
        BlockMetadata meta;
        char data[BLOCK_SIZE];
        if (readBlock(i, meta, data)) {
            if (meta.blockNumber == 0 && meta.fileId > 0) {
                // Found a file's first block
                FileEntry* f = loadFile(meta.fileId);
                if (f) {
                    fm.loadFileEntry(*f);
                    filesLoaded++;
                    delete f;
                }
            }
        }
    }
    
    std::cout << "[Disk] Loaded " << filesLoaded << " files from disk\n";
    return true;
}

// Delete file from disk
bool FileManagerDisk::deleteFile(int fileId) {
    try {
        // Get all blocks
        std::vector<int> blocks = getFileBlocks(fileId);
        
        if (blocks.empty()) {
            return false;
        }
        
        // Free all blocks
        for (int block : blocks) {
            freeBlock(block);
        }
        
        saveBitmap();
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[Disk Error] Failed to delete file: " << e.what() << "\n";
        return false;
    }
}

// Update file (delete + save)
bool FileManagerDisk::updateFile(const FileEntry& f) {
    deleteFile(f.fileId);
    return saveFile(f);
}

// Print disk statistics
void FileManagerDisk::printDiskStats() const {
    int used = getUsedBlocks();
    int free = getFreeBlocks();
    float usedMB = (used * BLOCK_SIZE) / (1024.0 * 1024.0);
    float freeMB = (free * BLOCK_SIZE) / (1024.0 * 1024.0);
    
    std::cout << "\n========== DISK STATISTICS ==========\n";
    std::cout << "Total Blocks: " << TOTAL_BLOCKS << "\n";
    std::cout << "Block Size: " << BLOCK_SIZE / 1024 << " KB\n";
    std::cout << "Used Blocks: " << used << " (" << usedMB << " MB)\n";
    std::cout << "Free Blocks: " << free << " (" << freeMB << " MB)\n";
    std::cout << "Disk Usage: " << (used * 100.0 / TOTAL_BLOCKS) << "%\n";
    std::cout << "=====================================\n\n";
}

int FileManagerDisk::getFreeBlocks() const {
    int count = 0;
    for (bool b : blockBitmap) if (!b) count++;
    return count;
}

int FileManagerDisk::getUsedBlocks() const {
    int count = 0;
    for (bool b : blockBitmap) if (b) count++;
    return count;
}
