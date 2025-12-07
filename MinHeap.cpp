#include "MinHeap.hpp"
#include <algorithm>

void MinHeap::heapifyUp(int index) {
    while (index > 0) {
        int parent = (index - 1) / 2;
        if (heap[parent].expireTime <= heap[index].expireTime) break;
        std::swap(heap[parent], heap[index]);
        index = parent;
    }
}

void MinHeap::heapifyDown(int index) {
    int n = (int)heap.size();
    while (true) {
        int left = 2 * index + 1;
        int right = 2 * index + 2;
        int smallest = index;

        if (left < n && heap[left].expireTime < heap[smallest].expireTime)
            smallest = left;
        if (right < n && heap[right].expireTime < heap[smallest].expireTime)
            smallest = right;

        if (smallest == index) break;
        std::swap(heap[index], heap[smallest]);
        index = smallest;
    }
}

void MinHeap::insert(int fileId, std::time_t expireTime) {
    heap.emplace_back(fileId, expireTime);
    heapifyUp((int)heap.size() - 1);
}

HeapNode MinHeap::peek() const {
    if (heap.empty()) return HeapNode(-1, -1);
    return heap.front();
}

HeapNode MinHeap::extractMin() {
    if (heap.empty()) return HeapNode(-1, -1);
    HeapNode minNode = heap.front();
    heap[0] = heap.back();
    heap.pop_back();
    if (!heap.empty()) heapifyDown(0);
    return minNode;
}

bool MinHeap::removeByFileId(int fileId) {
    int n = (int)heap.size();
    int idx = -1;
    for (int i = 0; i < n; ++i) {
        if (heap[i].fileId == fileId) { idx = i; break; }
    }
    if (idx == -1) return false;
    // replace with last and pop
    heap[idx] = heap.back();
    heap.pop_back();
    if (idx < (int)heap.size()) {
        heapifyUp(idx);
        heapifyDown(idx);
    }
    return true;
}

bool MinHeap::isEmpty() const {
    return heap.empty();
}

size_t MinHeap::size() const {
    return heap.size();
}

void MinHeap::display() const {
    std::cout << "MinHeap [fileId, expireTime] (size=" << heap.size() << "):\n";
    for (const auto &n : heap) {
        std::cout << "  (" << n.fileId << ", " << n.expireTime << ")  ";
    }
    std::cout << "\n";
}
