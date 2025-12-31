#ifndef HASHMAP_HPP
#define HASHMAP_HPP


#include <vector>
#include <iostream>
#include <string>
#include <type_traits>
#include "FileEntry.hpp"
#include "User.hpp"


template<typename T>
class HashMap {
private:
    std::vector<T> table;
    int capacity;
    int count;
    
    int hashFunction(int key) {
        return key % capacity;
    }
    
    int hashFunction(const std::string& key) {
        int hash = 0;
        for (char c : key) {
            hash = (hash * 31 + c) % capacity;
        }
        return hash;
    }
    
    void rehash() {
        int oldCapacity = capacity;
        capacity *= 2;
        std::vector<T> oldTable = table;
        table.clear();
        table.resize(capacity);
        count = 0;
        
        for (auto &entry : oldTable) {
            if (entry.inUse)
                insert(entry);
        }
    }

public:
    HashMap(int initialSize = 10) : capacity(initialSize), count(0) {
        table.resize(capacity);
    }
    
    bool insert(const T &item) {
        if (count >= capacity * 0.7) rehash();
        
        int idx;
        if constexpr (std::is_same<T, FileEntry>::value) {
            idx = hashFunction(item.fileId);
            while (table[idx].inUse) {
                if (table[idx].fileId == item.fileId)
                    return false;
                idx = (idx + 1) % capacity;
            }
        } else { 
            idx = hashFunction(item.username);
            while (table[idx].inUse) {
                if (table[idx].username == item.username)
                    return false;
                idx = (idx + 1) % capacity;
            }
        }
        
        table[idx] = item;
        table[idx].inUse = true;
        count++;
        return true;
    }
    
    T* search(int key) {
        int idx = hashFunction(key);
        int startIdx = idx;
        
        while (table[idx].inUse) {
            if constexpr (std::is_same<T, FileEntry>::value) {
                if (table[idx].fileId == key)
                    return &table[idx];
            } else { 
                if (table[idx].userId == key)
                    return &table[idx];
            }
            idx = (idx + 1) % capacity;
            if (idx == startIdx) break;
        }
        return nullptr;
    }
    
    T* search(const std::string& key) {
        int idx = hashFunction(key);
        int startIdx = idx;
        
        while (table[idx].inUse) {
            if constexpr (std::is_same<T, User>::value) {
                if (table[idx].username == key)
                    return &table[idx];
            }
            idx = (idx + 1) % capacity;
            if (idx == startIdx) break;
        }
        return nullptr;
    }
    
    bool remove(int key) {
        T* item = search(key);
        if (!item) return false;
        item->inUse = false;
        count--;
        return true;
    }
    bool remove(const std::string& key) {
    T* item = search(key);
    if (!item) return false;
    item->inUse = false;
    count--;
    return true;
}

    bool edit(int key, const std::string &newContent) {
        T* item = search(key);
        if (!item) return false;
        if constexpr (std::is_same<T, FileEntry>::value) {
            item->content = newContent;
        }
        return true;
    }
    
    std::vector<T> getAll() const {
        std::vector<T> all;
        for (const auto &entry : table) {
            if (entry.inUse)
                all.push_back(entry);
        }
        return all;
    }
    
    void display() const {
        for (const auto &item : table) {
            if (item.inUse) {
                if constexpr (std::is_same<T, FileEntry>::value) {
                    std::cout << "ID: " << item.fileId
                              << ", Name: " << item.name
                              << ", Created: " << item.createTime
                              << ", Expires: " << item.expireTime
                              << ", Content: " << item.content
                              << "\n";
                } else { 
                    std::cout << "ID: " << item.userId
                              << ", Username: " << item.username
                              << "\n";
                }
            }
        }
    }
};

#endif 
