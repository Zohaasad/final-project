
#ifndef MINHEAP_HPP
#define MINHEAP_HPP

#include <vector>
#include <algorithm>
#include "FileEntry.hpp"


class FileEntryHeap {
private:
    std::vector<FileEntry*> heap;

    void heapifyUp(int idx) {
        while (idx > 0) {
            int parent = (idx - 1) / 2;
            if (heap[parent]->expireTime <= heap[idx]->expireTime) break;
            std::swap(heap[parent], heap[idx]);
            idx = parent;
        }
    }

    void heapifyDown(int idx) {
        int n = heap.size();
        while (true) {
            int left = 2 * idx + 1;
            int right = 2 * idx + 2;
            int smallest = idx;

            if (left < n && heap[left]->expireTime < heap[smallest]->expireTime)
                smallest = left;
            if (right < n && heap[right]->expireTime < heap[smallest]->expireTime)
                smallest = right;

            if (smallest == idx) break;
            std::swap(heap[idx], heap[smallest]);
            idx = smallest;
        }
    }

public:
    FileEntryHeap() = default;

    void push(FileEntry* f) {
        heap.push_back(f);
        heapifyUp(heap.size() - 1);
    }

    FileEntry* peek() const {
        if (heap.empty()) return nullptr;
        return heap.front();
    }

    FileEntry* extractMin() {
        if (heap.empty()) return nullptr;
        FileEntry* minF = heap.front();
        heap[0] = heap.back();
        heap.pop_back();
        if (!heap.empty()) heapifyDown(0);
        return minF;
    }

    void remove(FileEntry* f) {
        auto it = std::find(heap.begin(), heap.end(), f);
        if (it == heap.end()) return;

        int idx = it - heap.begin();
        heap[idx] = heap.back();
        heap.pop_back();
        if (idx < heap.size()) {
            heapifyUp(idx);
            heapifyDown(idx);
        }
    }
 
void update(FileEntry* f) {
    auto it = std::find(heap.begin(), heap.end(), f);
    if (it == heap.end()) return;

    int idx = it - heap.begin();
    heapifyUp(idx);
    heapifyDown(idx);
}


    bool isEmpty() const { return heap.empty(); }
    size_t size() const { return heap.size(); }
};

#endif

