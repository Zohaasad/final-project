#ifndef MINHEAP_HPP
#define MINHEAP_HPP

#include <vector>
#include <ctime>
#include <iostream>

struct HeapNode {
    int fileId;        
    std::time_t expireTime; 

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

   
    void insert(int fileId, std::time_t expireTime);
    HeapNode peek() const;
    HeapNode extractMin();
    bool removeByFileId(int fileId);

    bool isEmpty() const;
    size_t size() const;
    void display() const;
};

#endif 
