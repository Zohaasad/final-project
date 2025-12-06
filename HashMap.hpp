#ifndef HASHMAP_HPP
#define HASHMAP_HPP

#include <vector>
#include <iostream>
#include "FileEntry.hpp"

class HashMap {
private:
    std::vector<FileEntry> table;
    int capacity;
    int count;

    int hashFunction(int key);
    void rehash();

public:
    HashMap(int initialSize = 10);

    bool insert(const FileEntry &file);
    FileEntry* search(int fileId);
    bool remove(int fileId);
    bool edit(int fileId, const std::string &newContent);

    std::vector<FileEntry> getAll() const;
    void display() const;
};

#endif
