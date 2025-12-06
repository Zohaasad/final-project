#include "HashMap.hpp"

// Constructor
HashMap::HashMap(int initialSize) : capacity(initialSize), count(0) {
    table.resize(capacity);
}

int HashMap::hashFunction(int key) {
    return key % capacity;
}

void HashMap::rehash() {
    int oldCapacity = capacity;
    capacity *= 2;

    std::vector<FileEntry> oldTable = table;
    table.clear();
    table.resize(capacity);
    count = 0;

    for (auto &entry : oldTable) {
        if (entry.inUse)
            insert(entry);
    }
}

bool HashMap::insert(const FileEntry &file) {
    if (count >= capacity * 0.7) rehash();

    int idx = hashFunction(file.fileId);

    while (table[idx].inUse) {
        if (table[idx].fileId == file.fileId)
            return false;
        idx = (idx + 1) % capacity;
    }

    table[idx] = file;
    table[idx].inUse = true;
    count++;
    return true;
}

FileEntry* HashMap::search(int fileId) {
    int idx = hashFunction(fileId);
    int startIdx = idx;

    while (table[idx].inUse) {
        if (table[idx].fileId == fileId)
            return &table[idx];

        idx = (idx + 1) % capacity;
        if (idx == startIdx) break;
    }
    return nullptr;
}

bool HashMap::remove(int fileId) {
    FileEntry* file = search(fileId);
    if (!file) return false;

    file->inUse = false;
    count--;
    return true;
}

bool HashMap::edit(int fileId, const std::string &newContent) {
    FileEntry* file = search(fileId);
    if (!file) return false;

    file->content = newContent;
    return true;
}

std::vector<FileEntry> HashMap::getAll() const {
    std::vector<FileEntry> all;

    for (const auto &entry : table) {
        if (entry.inUse)
            all.push_back(entry);
    }

    return all;
}

void HashMap::display() const {
    for (const auto &f : table) {
        if (f.inUse) {
            std::cout << "ID: " << f.fileId
                      << ", Name: " << f.name
                      << ", Created: " << f.createTime
                      << ", Expires: " << f.expireTime
                      << ", Content: " << f.content
                      << "\n";
        }
    }
}
