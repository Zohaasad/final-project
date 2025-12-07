#include "BTree.hpp"
#include <ctime>

// ================== BTreeNode ==================
BTreeNode::BTreeNode(int _t, bool _isLeaf) {
    t = _t;
    isLeaf = _isLeaf;
    n = 0;
    keys = new FileEntry[2*t-1];
    childrenOffsets = new long[2*t];
    offset = -1;
}

BTreeNode::~BTreeNode() {
    delete[] keys;
    delete[] childrenOffsets;
}

void BTreeNode::writeNode(std::fstream &file) {
    if (offset == -1) {
        file.seekp(0, std::ios::end);
        offset = file.tellp();
    } else {
        file.seekp(offset);
    }
    file.write(reinterpret_cast<char*>(&isLeaf), sizeof(isLeaf));
    file.write(reinterpret_cast<char*>(&n), sizeof(n));
    file.write(reinterpret_cast<char*>(keys), sizeof(FileEntry)*(2*t-1));
    file.write(reinterpret_cast<char*>(childrenOffsets), sizeof(long)*(2*t));
}

void BTreeNode::readNode(std::fstream &file, long pos) {
    file.seekg(pos);
    offset = pos;
    file.read(reinterpret_cast<char*>(&isLeaf), sizeof(isLeaf));
    file.read(reinterpret_cast<char*>(&n), sizeof(n));
    file.read(reinterpret_cast<char*>(keys), sizeof(FileEntry)*(2*t-1));
    file.read(reinterpret_cast<char*>(childrenOffsets), sizeof(long)*(2*t));
}

FileEntry* BTreeNode::search(int id, std::fstream &file) {
    int i=0;
    while(i<n && id>keys[i].fileId) i++;
if(i<n && keys[i].fileId==id) return &keys[i];
    if(isLeaf) return nullptr;
    BTreeNode* child = new BTreeNode(t,true);
    child->readNode(file, childrenOffsets[i]);
    FileEntry* res = child->search(id, file);
    delete child;
    return res;
}

void BTreeNode::traverse(std::fstream &file) {
    int i;
    for(i=0;i<n;i++){
        if(!isLeaf){
            BTreeNode* child = new BTreeNode(t,true);
            child->readNode(file, childrenOffsets[i]);
            child->traverse(file);
            delete child;
        }
        std::cout << "ID: " << keys[i].fileId << ", Name: " << keys[i].name
                  << ", Content: " << keys[i].content << "\n";
    }
    if(!isLeaf){
        BTreeNode* child = new BTreeNode(t,true);
        child->readNode(file, childrenOffsets[i]);
        child->traverse(file);
        delete child;
    }
}

void BTreeNode::splitChild(int i, BTreeNode* y, std::fstream &file) {
    BTreeNode* z = new BTreeNode(y->t, y->isLeaf);
    z->n = t-1;
    for(int j=0;j<t-1;j++) z->keys[j]=y->keys[j+t];
    if(!y->isLeaf){
        for(int j=0;j<t;j++) z->childrenOffsets[j]=y->childrenOffsets[j+t];
    }
    y->n=t-1;
    for(int j=n;j>i;j--) childrenOffsets[j+1]=childrenOffsets[j];
    childrenOffsets[i+1]=z->offset;
    for(int j=n-1;j>=i;j--) keys[j+1]=keys[j];
    keys[i]=y->keys[t-1];
    n++;
    y->writeNode(file);
    z->writeNode(file);
    writeNode(file);
    delete z;
}

void BTreeNode::insertNonFull(const FileEntry &entry, std::fstream &file) {
    int i = n-1;
    if(isLeaf){
       while(i>=0 && keys[i].fileId>entry.fileId){ keys[i+1]=keys[i]; i--; }
        keys[i+1]=entry;
        n++;
        writeNode(file);
    } else {
while(i>=0 && keys[i].fileId>entry.fileId) i--;
        i++;
        BTreeNode* child = new BTreeNode(t,true);
        child->readNode(file, childrenOffsets[i]);
        if(child->n==2*t-1){
            splitChild(i, child, file);
           if(keys[i].fileId<entry.fileId) i++;
            child->readNode(file, childrenOffsets[i]);
        }
        child->insertNonFull(entry, file);
        delete child;
    }
}

// ================== BTree ==================
BTree::BTree(int _t, const std::string &_filename) {
    t = _t;
    filename = _filename;
    root = new BTreeNode(t,true);
    file.open(filename,std::ios::in|std::ios::out|std::ios::binary|std::ios::app);
    if(!file.is_open()) file.open(filename,std::ios::out|std::ios::binary);
    root->writeNode(file);
}

BTree::~BTree() { if(file.is_open()) file.close(); delete root; }

void BTree::insert(const FileEntry &entry) {
    if(root->n==2*t-1){
        BTreeNode* s = new BTreeNode(t,false);
        s->childrenOffsets[0]=root->offset;
        s->splitChild(0, root, file);
        root=s;
        root->insertNonFull(entry, file);
    } else root->insertNonFull(entry, file);
}

FileEntry* BTree::search(int id) { return root->search(id, file); }

void BTree::traverse() { root->traverse(file); }
