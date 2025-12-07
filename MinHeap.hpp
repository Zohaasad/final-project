#ifndef MINHEAP_HPP
#define MINHEAP_HPP

#include <vector>
#include <ctime>
#include <iostream>

struct HeapNode {
    int fileId;        // reference to file in hash table
    std::time_t expireTime; // unix timestamp when file should expire

    HeapNode() : fileId(-1), expireTime(0) {}
    HeapNode(int fid, std::time_t et) : fileId(fid), expireTime(et) {}
};

class MinHeap {
private:
    std::vector<HeapNode> heap;

    void heapifyUp(int index);
    void heapifyDown(int index);

public:
    MinHeap() = default;

    // insert a node (fileId, expireTime)
    void insert(int fileId, std::time_t expireTime);

    // peek minimum (earliest expire) - returns { -1, -1 } if empty
    HeapNode peek() const;

    // remove and return min node - returns { -1, -1 } if empty
    HeapNode extractMin();

    // remove arbitrary node by fileId (linear scan) - returns true if removed
    bool removeByFileId(int fileId);

    bool isEmpty() const;
    size_t size() const;

    // debug
    void display() const;
};

#endif // MINHEAP_HPP
