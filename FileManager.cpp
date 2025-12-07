#include "FileManager.hpp"
#include "FileManagerDisk.hpp"
#include <iostream>
#include <ctime>
FileManager::FileManager(int initialSize)
    : files(initialSize), binFiles(initialSize), nextFileId(1), disk(nullptr), currentUserId(-1) {}

