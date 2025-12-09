
#pragma once
#include <fstream>
#include <vector>
#include <iostream>

struct FileIndexEntry {
    int fileId;
    int firstBlock;
    bool inUse;

    FileIndexEntry(int fId = 0, int fBlock = -1) : fileId(fId), firstBlock(fBlock), inUse(true) {}
};

class BTreeNode {
public:
    int t; // minimum degree
    bool isLeaf;
    int n; // number of keys
    FileIndexEntry* keys;
    long* childrenOffsets;
    long offset; // position in file

    BTreeNode(int _t, bool _isLeaf);
    ~BTreeNode();

    void writeNode(std::fstream &file);
    void readNode(std::fstream &file, long pos);

    FileIndexEntry* search(int fileId, std::fstream &file);
    void traverse(std::fstream &file);
    void splitChild(int i, BTreeNode* y, std::fstream &file);
    void insertNonFull(const FileIndexEntry &entry, std::fstream &file);
    bool removeKey(int fileId, std::fstream &file);
    void collectFileIds(std::fstream &file, std::vector<int>& ids);
};

class BTree {
public:
    int t;
    std::string filename;
    BTreeNode* root;
    std::fstream file;

    BTree(int _t, const std::string &_filename);
    ~BTree();

    void insert(int fileId, int firstBlock);
    FileIndexEntry* search(int fileId);
    bool remove(int fileId);
    std::vector<int> getAllFileIds();
    void traverse();
private:
    long rootOffset; // stored in file header
    void readRoot();
    void writeRoot();
};
