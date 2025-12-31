
#include "FileManagerDisk.hpp"
#include "FileManager.hpp"
#include <iostream>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <ctime>

FileManagerDisk::FileManagerDisk(const std::string& diskPath)
    : diskFilePath(diskPath), blockBitmap(TOTAL_BLOCKS, false), totalBlocks(TOTAL_BLOCKS), usedBlocks(0)
{
    std::cout << "\n[FileManagerDisk] Initializing disk subsystem...\n";
    
    if (!initializeDisk()) {
        std::cerr << "[ERROR] Disk initialization failed.\n";
        exit(1);
    }

    btree = new BTree(3, "btree.dat");
    std::cout << "[FileManagerDisk] B-Tree index loaded from disk.\n";

    loadBitmap();

    usedBlocks = 0;
    for (bool b : blockBitmap) {
        if (b) usedBlocks++;
    }
    
    float usedMB = (usedBlocks * BLOCK_SIZE) / (1024.0 * 1024.0);
    float totalMB = (totalBlocks * BLOCK_SIZE) / (1024.0 * 1024.0);
    
    std::cout << "[FileManagerDisk] Disk initialized successfully.\n";
    std::cout << "[FileManagerDisk] Used blocks: " << usedBlocks << "/" << totalBlocks 
              << " (" << usedMB << " MB / " << totalMB << " MB)\n";
}

FileManagerDisk::~FileManagerDisk() {
    std::cout << "[FileManagerDisk] Shutting down disk subsystem...\n";
    saveBitmap();
    if (diskFile.is_open()) diskFile.close();
    if (btree) delete btree;
    std::cout << "[FileManagerDisk] Disk subsystem closed.\n";
}

bool FileManagerDisk::initializeDisk() {
    diskFile.open(diskFilePath, std::ios::in | std::ios::out | std::ios::binary);
    
    if (!diskFile.is_open()) {
        std::cout << "[FileManagerDisk] No existing disk found. Creating new disk file...\n";
        
        std::ofstream creator(diskFilePath, std::ios::binary);
        if (!creator.is_open()) {
            std::cerr << "[ERROR] Cannot create disk file at: " << diskFilePath << "\n";
            return false;
        }

        char zero[BLOCK_SIZE] = {0};
        for (int i = 0; i < TOTAL_BLOCKS; i++) {
            creator.write(zero, BLOCK_SIZE);
            if (i % 1000 == 0) {
                std::cout << "[FileManagerDisk] Formatting disk... " 
                          << (i * 100 / TOTAL_BLOCKS) << "%\r" << std::flush;
            }
        }
        creator.close();
        std::cout << "[FileManagerDisk] Disk formatting complete.          \n";

        diskFile.open(diskFilePath, std::ios::in | std::ios::out | std::ios::binary);
        if (!diskFile.is_open()) {
            std::cerr << "[ERROR] Cannot open newly created disk file.\n";
            return false;
        }
    } else {
        std::cout << "[FileManagerDisk] Existing disk file found and opened.\n";
    }
    
    return true;
}

void FileManagerDisk::saveBitmap() {
    std::ofstream bmp("bitmap.dat", std::ios::binary);
    if (!bmp.is_open()) {
        std::cerr << "[ERROR] Failed to save bitmap!\n";
        return;
    }
    
    int header = usedBlocks;
    bmp.write(reinterpret_cast<char*>(&header), sizeof(int));
    
    for (bool b : blockBitmap) {
        char val = b ? 1 : 0;
        bmp.write(&val, 1);
    }
    bmp.close();
    
    std::cout << "[Bitmap] Saved to disk. Used blocks: " << usedBlocks << "/" << totalBlocks << "\n";
}

void FileManagerDisk::loadBitmap() {
    std::ifstream bmp("bitmap.dat", std::ios::binary);
    if (!bmp.is_open()) {
        std::cout << "[Bitmap] No existing bitmap found. Starting with clean disk.\n";
        return;
    }
    
    int header = 0;
    bmp.read(reinterpret_cast<char*>(&header), sizeof(int));
    
    int loadedBlocks = 0;
    for (int i = 0; i < TOTAL_BLOCKS; i++) {
        char val;
        if (!bmp.read(&val, 1)) break;
        blockBitmap[i] = (val != 0);
        if (blockBitmap[i]) loadedBlocks++;
    }
    bmp.close();
    
    if (header != loadedBlocks) {
        std::cerr << "[WARNING] Bitmap header mismatch! Header: " << header 
                  << ", Actual: " << loadedBlocks << "\n";
    }
    
    std::cout << "[Bitmap] Loaded from disk. " << loadedBlocks << " blocks marked as used.\n";
}

int FileManagerDisk::allocateBlock() {
    for (int i = 0; i < TOTAL_BLOCKS; i++) {
        if (!blockBitmap[i]) {
            blockBitmap[i] = true;
            usedBlocks++;
            std::cout << "[Disk] Allocated block #" << i << " (used: " << usedBlocks << ")\n";
            return i;
        }
    }
    
    std::cerr << "[ERROR] DISK FULL! No free blocks available. Used: " << usedBlocks << "/" << totalBlocks << "\n";
    return -1;
}

void FileManagerDisk::freeBlock(int blockNum) {
    if (blockNum < 0 || blockNum >= TOTAL_BLOCKS) {
        std::cerr << "[ERROR] Invalid block number: " << blockNum << "\n";
        return;
    }
    
    if (!blockBitmap[blockNum]) {
        std::cerr << "[WARNING] Attempting to free already free block #" << blockNum << "\n";
        return;
    }
    
    blockBitmap[blockNum] = false;
    usedBlocks--;
    
    char zero[BLOCK_SIZE] = {0};
    diskFile.seekp(blockNum * BLOCK_SIZE, std::ios::beg);
    diskFile.write(zero, BLOCK_SIZE);
    diskFile.flush();
    
    std::cout << "[Disk] Freed block #" << blockNum << " (used: " << usedBlocks << ")\n";
}

bool FileManagerDisk::writeBlock(int blockNum, const BlockMetadata& meta, const char* data, int dataSize) {
    if (blockNum < 0 || blockNum >= TOTAL_BLOCKS) {
        std::cerr << "[ERROR] Invalid block number for write: " << blockNum << "\n";
        return false;
    }
    
    if (dataSize > BLOCK_SIZE - sizeof(BlockMetadata)) {
        std::cerr << "[ERROR] Data size exceeds block capacity: " << dataSize << "\n";
        return false;
    }
    
    diskFile.seekp(blockNum * BLOCK_SIZE, std::ios::beg);
    diskFile.write(reinterpret_cast<const char*>(&meta), sizeof(BlockMetadata));
    diskFile.write(data, dataSize);
    diskFile.flush();
    
    return true;
}

bool FileManagerDisk::readBlock(int blockNum, BlockMetadata& meta, char* data) {
    if (blockNum < 0 || blockNum >= TOTAL_BLOCKS) {
        std::cerr << "[ERROR] Invalid block number for read: " << blockNum << "\n";
        return false;
    }
    
    diskFile.seekg(blockNum * BLOCK_SIZE, std::ios::beg);
    diskFile.read(reinterpret_cast<char*>(&meta), sizeof(BlockMetadata));
    diskFile.read(data, meta.dataSize);
    
    if (diskFile.fail()) {
        std::cerr << "[ERROR] Failed to read block #" << blockNum << "\n";
        return false;
    }
    
    return true;
}

std::vector<int> FileManagerDisk::getFileBlocks(int fileId) {
    std::vector<int> blocks;
    
    FileIndexEntry* entry = btree->search(fileId);
    if (!entry) {
        return blocks;
    }
    
    int blockNum = entry->firstBlock;
    int safetyCounter = 0;
    
    while (blockNum != -1 && safetyCounter < TOTAL_BLOCKS) {
        blocks.push_back(blockNum);
        
        BlockMetadata meta;
        char buffer[BLOCK_SIZE] = {0};
        
        if (!readBlock(blockNum, meta, buffer)) {
            std::cerr << "[ERROR] Failed to read block chain at block #" << blockNum << "\n";
            break;
        }
        
        blockNum = meta.nextBlock;
        safetyCounter++;
    }
    
    if (safetyCounter >= TOTAL_BLOCKS) {
        std::cerr << "[ERROR] Infinite loop detected in block chain for file " << fileId << "\n";
    }
    
    return blocks;
}

bool FileManagerDisk::saveFile(const FileEntry& f) {
    std::cout << "\n[Disk] ===== Saving File =====\n";
    std::cout << "[Disk] File ID: " << f.fileId << "\n";
    std::cout << "[Disk] Name: " << f.name << "\n";
    std::cout << "[Disk] Content size: " << f.content.size() << " bytes\n";
    
  
    std::vector<int> existingBlocks = getFileBlocks(f.fileId);
    
    if (!existingBlocks.empty()) {
        std::cout << "[Disk] File already exists with " << existingBlocks.size() << " blocks. Will reuse them.\n";
    } else {
        std::cout << "[Disk] New file - no existing blocks.\n";
    }

 
    char metaBuffer[256] = {0};
    int offset = 0;
    
    std::memcpy(metaBuffer + offset, &f.fileId, sizeof(int)); 
    offset += sizeof(int);
    
    std::memcpy(metaBuffer + offset, &f.userId, sizeof(int)); 
    offset += sizeof(int);
    
    std::memcpy(metaBuffer + offset, &f.ownerId, sizeof(int)); 
    offset += sizeof(int);
    
    int nameLen = f.name.size();
    std::memcpy(metaBuffer + offset, &nameLen, sizeof(int)); 
    offset += sizeof(int);
    std::memcpy(metaBuffer + offset, f.name.data(), nameLen); 
    offset += nameLen;
    
    std::memcpy(metaBuffer + offset, &f.createTime, sizeof(time_t)); 
    offset += sizeof(time_t);
    
    std::memcpy(metaBuffer + offset, &f.expireTime, sizeof(time_t)); 
    offset += sizeof(time_t);
    
    std::memcpy(metaBuffer + offset, &f.inBin, sizeof(bool)); 
    offset += sizeof(bool);
    
    std::memcpy(metaBuffer + offset, &f.inUse, sizeof(bool)); 
    offset += sizeof(bool);

    std::string totalData(metaBuffer, offset);
    totalData += f.content;

    size_t totalSize = totalData.size();
    size_t dataPerBlock = BLOCK_SIZE - sizeof(BlockMetadata);
    size_t blocksNeeded = (totalSize + dataPerBlock - 1) / dataPerBlock; 

    std::cout << "[Disk] Total data size: " << totalSize << " bytes\n";
    std::cout << "[Disk] Blocks needed: " << blocksNeeded << "\n";
    std::cout << "[Disk] Existing blocks: " << existingBlocks.size() << "\n";

    std::vector<int> blocksToUse;
    
    for (size_t i = 0; i < blocksNeeded && i < existingBlocks.size(); i++) {
        blocksToUse.push_back(existingBlocks[i]);
        std::cout << "[Disk] ✓ Reusing block #" << existingBlocks[i] << "\n";
    }
    
    if (blocksNeeded > existingBlocks.size()) {
        size_t newBlocksNeeded = blocksNeeded - existingBlocks.size();
        std::cout << "[Disk] Need " << newBlocksNeeded << " NEW blocks.\n";
        
        for (size_t i = 0; i < newBlocksNeeded; i++) {
            int newBlock = allocateBlock();
            if (newBlock == -1) {
                std::cerr << "[ERROR] Disk full! Cannot allocate more blocks.\n";
                return false;
            }
            blocksToUse.push_back(newBlock);
        }
    }

    if (blocksNeeded < existingBlocks.size()) {
        size_t blocksToFree = existingBlocks.size() - blocksNeeded;
        std::cout << "[Disk] File shrunk. Freeing " << blocksToFree << " unused blocks.\n";
        
        for (size_t i = blocksNeeded; i < existingBlocks.size(); i++) {
            freeBlock(existingBlocks[i]);
        }
    }
    size_t written = 0;
    
    for (size_t i = 0; i < blocksToUse.size(); i++) {
        int blockNum = blocksToUse[i];
        size_t remainingData = totalSize - written;
        size_t writeSize = std::min(remainingData, dataPerBlock);

        BlockMetadata meta;
        meta.fileId = f.fileId;
        meta.blockNumber = i;
        meta.nextBlock = (i + 1 < blocksToUse.size()) ? blocksToUse[i + 1] : -1;
        meta.dataSize = writeSize;

        char buffer[BLOCK_SIZE] = {0};
        std::memcpy(buffer, totalData.data() + written, writeSize);
       
        if (!writeBlock(blockNum, meta, buffer, writeSize)) {
            std::cerr << "[ERROR] Failed to write block #" << blockNum << "\n";
            return false;
        }

        std::cout << "[Disk] Wrote " << writeSize << " bytes to block #" << blockNum 
                  << " (next: " << meta.nextBlock << ")\n";

        written += writeSize;
    }

    btree->insert(f.fileId, blocksToUse[0]);
    std::cout << "[Disk] B-tree index updated: File " << f.fileId << " -> Block " << blocksToUse[0] << "\n";

    saveBitmap();

    std::cout << "[Disk] ===== Save Complete =====\n";
    std::cout << "[Disk] Used " << blocksToUse.size() << " blocks for file " << f.fileId << "\n";
    std::cout << "[Disk] Disk usage: " << usedBlocks << "/" << totalBlocks << " blocks\n\n";
    
    return true;
}

FileEntry* FileManagerDisk::loadFile(int fileId) {
    std::cout << "[Disk] Loading file ID " << fileId << "...\n";
    
    std::vector<int> blocks = getFileBlocks(fileId);
    
    if (blocks.empty()) {
        std::cerr << "[Disk] No blocks found for file ID " << fileId << "\n";
        return nullptr;
    }
    
    std::cout << "[Disk] Found " << blocks.size() << " blocks for file " << fileId << "\n";

    std::string totalData;
    
    for (int blockNum : blocks) {
        BlockMetadata meta;
        char buffer[BLOCK_SIZE] = {0};
        
        if (!readBlock(blockNum, meta, buffer)) {
            std::cerr << "[ERROR] Failed to read block #" << blockNum << "\n";
            return nullptr;
        }
        
        totalData.append(buffer, meta.dataSize);
    }

    size_t pos = 0;
    FileEntry* f = new FileEntry();

    std::memcpy(&f->fileId, totalData.data() + pos, sizeof(int)); 
    pos += sizeof(int);
    
    std::memcpy(&f->userId, totalData.data() + pos, sizeof(int)); 
    pos += sizeof(int);
    
    std::memcpy(&f->ownerId, totalData.data() + pos, sizeof(int)); 
    pos += sizeof(int);
    
    int nameLen = 0;
    std::memcpy(&nameLen, totalData.data() + pos, sizeof(int)); 
    pos += sizeof(int);
    
    f->name = totalData.substr(pos, nameLen); 
    pos += nameLen;
    
    std::memcpy(&f->createTime, totalData.data() + pos, sizeof(time_t)); 
    pos += sizeof(time_t);
    
    std::memcpy(&f->expireTime, totalData.data() + pos, sizeof(time_t)); 
    pos += sizeof(time_t);
    
    std::memcpy(&f->inBin, totalData.data() + pos, sizeof(bool)); 
    pos += sizeof(bool);
    
    std::memcpy(&f->inUse, totalData.data() + pos, sizeof(bool)); 
    pos += sizeof(bool);

    f->content = totalData.substr(pos);
    f->expired = false;

    std::cout << "[Disk] Loaded file: " << f->name << " (" << f->content.size() << " bytes)\n";
    
    return f;
}

bool FileManagerDisk::deleteFile(int fileId) {
    std::cout << "\n[Disk] ===== Deleting File =====\n";
    std::cout << "[Disk] File ID: " << fileId << "\n";
  
    std::vector<int> blocks = getFileBlocks(fileId);
    
    if (blocks.empty()) {
        std::cerr << "[WARNING] No blocks found for file " << fileId << ". Already deleted?\n";
        btree->remove(fileId); 
        return true;
    }
    
    std::cout << "[Disk] Found " << blocks.size() << " blocks to delete.\n";
    
    for (int blockNum : blocks) {
        freeBlock(blockNum);
    }
    
    btree->remove(fileId);
    std::cout << "[Disk] Removed from B-tree index.\n";
    
    saveBitmap();
    
    std::cout << "[Disk] ===== Delete Complete =====\n";
    std::cout << "[Disk] Freed " << blocks.size() << " blocks.\n";
    std::cout << "[Disk] Disk usage: " << usedBlocks << "/" << totalBlocks << " blocks\n\n";
    
    return true;
}

bool FileManagerDisk::updateFile(const FileEntry& f) {
    return saveFile(f);
}

bool FileManagerDisk::loadAllFiles(FileManager& fm) {
    std::cout << "\n[Disk] ========== Loading All Files ==========\n";
    
    std::vector<int> allFileIds = btree->getAllFileIds();
    std::cout << "[Disk] Found " << allFileIds.size() << " files in B-tree index.\n";
    
    if (allFileIds.empty()) {
        std::cout << "[Disk] No files to load. Starting with empty file system.\n";
        return true;
    }
    
    time_t now = std::time(nullptr);
    int successCount = 0;
    int failCount = 0;

    for (int fileId : allFileIds) {
        FileEntry* f = loadFile(fileId);
        
        if (!f) {
            std::cerr << "[ERROR] Failed to load file ID " << fileId << "\n";
            failCount++;
            continue;
        }

        bool shouldBeInBin = f->inBin || (f->expireTime <= now);
        long remainingTime = shouldBeInBin ? 0 : std::max(1L, f->expireTime - now);

        if (fm.restoreFile(
            f->fileId,
            f->name,
            f->content,
            remainingTime,
            f->userId,
            shouldBeInBin,
            f->createTime,
            f->expireTime
        )) {
            successCount++;
        } else {
            std::cerr << "[WARNING] Failed to restore file ID " << fileId << " to FileManager.\n";
            failCount++;
        }

        delete f;
    }

    std::cout << "[Disk] ========== Load Complete ==========\n";
    std::cout << "[Disk] Successfully loaded: " << successCount << " files\n";
    if (failCount > 0) {
        std::cout << "[Disk] Failed to load: " << failCount << " files\n";
    }
    std::cout << "\n";
    
    return true;
}

int FileManagerDisk::getUsedBlocks() const { 
    return usedBlocks; 
}

int FileManagerDisk::getFreeBlocks() const { 
    return totalBlocks - usedBlocks; 
}
