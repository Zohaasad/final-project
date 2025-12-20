
#include "BTree.hpp"

BTreeNode::BTreeNode(int _t, bool _isLeaf) {
    t = _t;
    isLeaf = _isLeaf;
    n = 0;
    keys = new FileIndexEntry[2*t - 1];
    childrenOffsets = new long[2*t];
    offset = -1;
    for (int i = 0; i < 2*t; i++) childrenOffsets[i] = -1;
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
    file.write(reinterpret_cast<char*>(keys), sizeof(FileIndexEntry)*(2*t -1));
    file.write(reinterpret_cast<char*>(childrenOffsets), sizeof(long)*(2*t));
    file.flush();
}

void BTreeNode::readNode(std::fstream &file, long pos) {
    if (pos < 0) return;
    file.seekg(pos);
    offset = pos;
    file.read(reinterpret_cast<char*>(&isLeaf), sizeof(isLeaf));
    file.read(reinterpret_cast<char*>(&n), sizeof(n));
    file.read(reinterpret_cast<char*>(keys), sizeof(FileIndexEntry)*(2*t -1));
    file.read(reinterpret_cast<char*>(childrenOffsets), sizeof(long)*(2*t));
}

FileIndexEntry* BTreeNode::search(int fileId, std::fstream &file) {
    int i = 0;
    while(i < n && fileId > keys[i].fileId) i++;

    if(i < n && keys[i].fileId == fileId && keys[i].inUse) {
        std::cout << "[BTree] Found file " << fileId << " at block " << keys[i].firstBlock << "\n";
        return &keys[i];
    }
    
    if(isLeaf) {
        std::cout << "[BTree] File " << fileId << " not found in leaf node\n";
        return nullptr;
    }

    if(childrenOffsets[i] == -1) {
        std::cout << "[BTree] File " << fileId << " not found (null child pointer)\n";
        return nullptr;
    }

    BTreeNode* child = new BTreeNode(t, true);
    child->readNode(file, childrenOffsets[i]);
    FileIndexEntry* res = child->search(fileId, file);
    delete child;
    return res;
}

void BTreeNode::traverse(std::fstream &file) {
    int i;
    for(i = 0; i < n; i++) {
        if(!isLeaf && childrenOffsets[i] != -1) {
            BTreeNode* child = new BTreeNode(t, true);
            child->readNode(file, childrenOffsets[i]);
            child->traverse(file);
            delete child;
        }
        if(keys[i].inUse) {
            std::cout << "FileID: " << keys[i].fileId << ", FirstBlock: " << keys[i].firstBlock << "\n";
        }
    }
    if(!isLeaf && childrenOffsets[i] != -1) {
        BTreeNode* child = new BTreeNode(t, true);
        child->readNode(file, childrenOffsets[i]);
        child->traverse(file);
        delete child;
    }
}

void BTreeNode::splitChild(int i, BTreeNode* y, std::fstream &file) {
    BTreeNode* z = new BTreeNode(y->t, y->isLeaf);
    z->n = t -1;

    for(int j = 0; j < t-1; j++) z->keys[j] = y->keys[j+t];
    if(!y->isLeaf) {
        for(int j = 0; j < t; j++) z->childrenOffsets[j] = y->childrenOffsets[j+t];
    }

    y->n = t-1;
    
    y->writeNode(file);
    z->writeNode(file);

    for(int j = n; j >= i+1; j--) childrenOffsets[j+1] = childrenOffsets[j];
    childrenOffsets[i+1] = z->offset;

    for(int j = n-1; j >= i; j--) keys[j+1] = keys[j];
    keys[i] = y->keys[t-1];

    n++;
    
    writeNode(file);

    delete z;
}

void BTreeNode::insertNonFull(const FileIndexEntry &entry, std::fstream &file) {
    int i = n-1;

    if(isLeaf) {
        // Check if this file ID already exists - UPDATE instead of insert
        for(int k = 0; k < n; k++) {
            if(keys[k].fileId == entry.fileId) {
                std::cout << "[BTree] ✓ UPDATING existing entry: File " << entry.fileId 
                          << " (block " << keys[k].firstBlock << " → " << entry.firstBlock << ")\n";
                keys[k].firstBlock = entry.firstBlock;
                keys[k].inUse = true;
                writeNode(file);
                return;
            }
        }
        
        // Not found - insert new entry
        while(i >=0 && keys[i].fileId > entry.fileId) { 
            keys[i+1] = keys[i]; 
            i--; 
        }
        keys[i+1] = entry; 
        n++;
        writeNode(file);
        std::cout << "[BTree] ✓ INSERTED new entry: File " << entry.fileId << " at block " << entry.firstBlock << "\n";
    } else {
        while(i >= 0 && keys[i].fileId > entry.fileId) i--;
        i++;
        
        if(childrenOffsets[i] == -1) return;
        
        BTreeNode* child = new BTreeNode(t,true);
        child->readNode(file, childrenOffsets[i]);
        
        if(child->n == 2*t-1) {
            splitChild(i, child, file);
            if(keys[i].fileId < entry.fileId) i++;
            delete child;
            child = new BTreeNode(t,true);
            child->readNode(file, childrenOffsets[i]);
        }
        
        child->insertNonFull(entry, file);
        delete child;
    }
}

bool BTreeNode::removeKey(int fileId, std::fstream &file) {
    int i=0;
    while(i<n && keys[i].fileId<fileId) i++;
    
    if(i<n && keys[i].fileId==fileId && keys[i].inUse) {
        keys[i].inUse = false;
        keys[i].firstBlock = -1;
        writeNode(file);
        std::cout << "[BTree] Marked file " << fileId << " as deleted\n";
        return true;
    }
    
    if(isLeaf) return false;
    if(childrenOffsets[i] == -1) return false;
    
    BTreeNode* child = new BTreeNode(t,true);
    child->readNode(file, childrenOffsets[i]);
    bool res = child->removeKey(fileId,file);
    delete child;
    return res;
}

void BTreeNode::collectFileIds(std::fstream &file, std::vector<int>& ids) {
    int i;
    for(i = 0; i < n; i++) {
        if(!isLeaf && childrenOffsets[i] != -1) {
            BTreeNode* child = new BTreeNode(t,true);
            child->readNode(file, childrenOffsets[i]);
            child->collectFileIds(file, ids);
            delete child;
        }
        if(keys[i].inUse) ids.push_back(keys[i].fileId);
    }
    if(!isLeaf && childrenOffsets[i] != -1) {
        BTreeNode* child = new BTreeNode(t,true);
        child->readNode(file, childrenOffsets[i]);
        child->collectFileIds(file, ids);
        delete child;
    }
}

// ==================== BTree Implementation ====================

BTree::BTree(int _t, const std::string &_filename) {
    t = _t;
    filename = _filename;
    root = nullptr;
    rootOffset = -1;

    file.open(filename, std::ios::in | std::ios::out | std::ios::binary);
    if(!file.is_open()) {
        std::cout << "[BTree] Creating new B-tree file: " << filename << "\n";
        file.open(filename, std::ios::out|std::ios::binary);
        file.close();
        file.open(filename, std::ios::in|std::ios::out|std::ios::binary);
    }

    file.seekg(0, std::ios::end);
    long fileSize = file.tellg();
    
    std::cout << "[BTree] B-tree file size: " << fileSize << " bytes\n";
    
    if(fileSize >= sizeof(long)) {
        // Read root offset from beginning of file
        file.seekg(0);
        file.read(reinterpret_cast<char*>(&rootOffset), sizeof(long));
        
        std::cout << "[BTree] Root offset read from disk: " << rootOffset << "\n";
        
        // Validate root offset
        if (rootOffset >= sizeof(long) && rootOffset < fileSize) {
            root = new BTreeNode(t,true);
            root->readNode(file, rootOffset);
            std::cout << "[BTree] ✓ Loaded existing B-tree root with " << root->n << " keys\n";
        } else {
            std::cout << "[BTree] Invalid root offset, creating new root\n";
            root = new BTreeNode(t,true);
            // Write root node AFTER the root offset header (at position sizeof(long))
            file.seekp(sizeof(long), std::ios::beg);
            root->offset = file.tellp();
            root->writeNode(file);
            rootOffset = root->offset;
            writeRoot();
        }
    } else {
        // New B-tree file
        std::cout << "[BTree] Initializing new B-tree\n";
        root = new BTreeNode(t,true);
        
        // Reserve space for root offset at beginning
        file.seekp(sizeof(long), std::ios::beg);
        root->offset = file.tellp();
        root->writeNode(file);
        rootOffset = root->offset;
        writeRoot();
        
        std::cout << "[BTree] ✓ New root created at offset: " << rootOffset << "\n";
    }
}

BTree::~BTree() {
    std::cout << "[BTree] Closing B-tree, root offset: " << rootOffset << "\n";
    if(file.is_open()) file.close();
    delete root;
}

void BTree::writeRoot() {
    // Write root offset at the very beginning of the file
    file.seekp(0);
    file.write(reinterpret_cast<char*>(&rootOffset), sizeof(long));
    file.flush();
    std::cout << "[BTree] Root offset saved to disk: " << rootOffset << "\n";
}

void BTree::insert(int fileId, int firstBlock) {
    std::cout << "\n[BTree] ===== Inserting/Updating file " << fileId << " -> block " << firstBlock << " =====\n";
    
    FileIndexEntry entry(fileId, firstBlock);

    // Check if root is full and needs splitting
    if(root->n == 2*t-1) {
        std::cout << "[BTree] Root is full (" << root->n << " keys), splitting...\n";
        
        // Create new root
        BTreeNode* newRoot = new BTreeNode(t, false);
        
        // Old root becomes first child of new root
        long oldRootOffset = root->offset;
        newRoot->childrenOffsets[0] = oldRootOffset;
        
        // Write new root to get its offset (after current end of file)
        file.seekp(0, std::ios::end);
        newRoot->offset = file.tellp();
        newRoot->writeNode(file);
        
        // Split the old root (which is now child[0] of new root)
        newRoot->splitChild(0, root, file);
        
        // Update root pointer
        rootOffset = newRoot->offset;
        delete root;
        root = newRoot;
        
        writeRoot();
        std::cout << "[BTree] ✓ New root created at offset: " << rootOffset << "\n";
    }

    // Now insert into the (possibly new) root
    root->insertNonFull(entry, file);
    
    // Ensure root is written
    root->writeNode(file);
    
    std::cout << "[BTree] ===== Insert/Update complete =====\n\n";
}

FileIndexEntry* BTree::search(int fileId) {
    if(!root) {
        std::cout << "[BTree] Search failed: No root node\n";
        return nullptr;
    }
    
    std::cout << "[BTree] Searching for file " << fileId << " (root has " << root->n << " keys)...\n";
    
    // Always read fresh root data from disk
    root->readNode(file, rootOffset);
    
    return root->search(fileId, file);
}

bool BTree::remove(int fileId) {
    if(!root) return false;
    
    std::cout << "[BTree] Removing file " << fileId << "\n";
    
    // Refresh root from disk
    root->readNode(file, rootOffset);
    
    return root->removeKey(fileId, file);
}

std::vector<int> BTree::getAllFileIds() {
    std::vector<int> ids;
    if(root) {
        // Refresh root from disk
        root->readNode(file, rootOffset);
        root->collectFileIds(file, ids);
    }
    std::cout << "[BTree] Collected " << ids.size() << " file IDs\n";
    return ids;
}

void BTree::traverse() {
    if(root) {
        std::cout << "\n========== B-Tree Index ==========\n";
        root->readNode(file, rootOffset);
        root->traverse(file);
        std::cout << "==================================\n\n";
    }
}
