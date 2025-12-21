

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
