
#ifndef BTREE_HPP
#define BTREE_HPP

#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <cstring>
#include "FileEntry.hpp"  

class BTreeNode {
public:
    bool isLeaf;
    int t;
    int n; 
    FileEntry* keys;
    long* childrenOffsets; 
    long offset;          
    BTreeNode(int _t, bool _isLeaf);
    ~BTreeNode();

    void traverse(std::fstream &file);
    FileEntry* search(int id, std::fstream &file);
    void insertNonFull(const FileEntry &entry, std::fstream &file);
    void splitChild(int i, BTreeNode* y, std::fstream &file);
    void writeNode(std::fstream &file);
    void readNode(std::fstream &file, long pos);
};

class BTree {
private:
    BTreeNode* root;
    int t;
    std::string filename;
    std::fstream file;

public:
    BTree(int _t, const std::string &_filename);
    ~BTree();

    void insert(const FileEntry &entry);
    FileEntry* search(int id);
    void traverse();
};

#endif
