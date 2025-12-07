#ifndef FILEENTRY_HPP
#define FILEENTRY_HPP

#include <string>
#include <ctime>

struct FileEntry {
    int fileId;
    std::string name;        // instead of fileName
    std::string content;

    time_t createTime;
    time_t expireTime;

    bool inBin = false;      // File is inside Recycle Bin
    bool inUse = false;      // Used by HashMap only
};

#endif
