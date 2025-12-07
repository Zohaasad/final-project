#ifndef QUEUE_HPP
#define QUEUE_HPP

#include <iostream>

template<typename T>
class Queue {
private:
    struct Node {
        T data;
        Node* next;
        Node(const T& val) : data(val), next(nullptr) {}
    };
    
    Node* frontNode;
    Node* rearNode;
    int count;

public:
    Queue() : frontNode(nullptr), rearNode(nullptr), count(0) {}
    
    ~Queue() {
        while (!isEmpty()) {
            dequeue();
        }
    }
    
    void enqueue(const T& value) {
        Node* newNode = new Node(value);
        if (isEmpty()) {
            frontNode = rearNode = newNode;
        } else {
            rearNode->next = newNode;
            rearNode = newNode;
        }
        count++;
    }
    
    T dequeue() {
        if (isEmpty()) {
            throw std::runtime_error("Queue is empty");
        }
        Node* temp = frontNode;
        T value = temp->data;
        frontNode = frontNode->next;
        if (frontNode == nullptr) {
            rearNode = nullptr;
        }
        delete temp;
        count--;
        return value;
    }
    
};

#endif 
