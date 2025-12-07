#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>
#include "FileManager.hpp"
#include "FileManagerDisk.hpp"
#include "MinHeap.hpp"

using namespace std;

// Global flag to control background thread
atomic<bool> keepRunning(true);
mutex heapMutex;
mutex fmMutex;

void printSeparator() {
    cout << "\n========================================\n";
}

void displayMenu() {
    printSeparator();
    cout << "FILE MANAGEMENT SYSTEM - MENU\n";
    printSeparator();
    cout << "1.  Create File\n";
    cout << "2.  Write/Edit File\n";
    cout << "3.  Read File\n";
    cout << "4.  Truncate File\n";
    cout << "5.  Move File to Bin\n";
    cout << "6.  Retrieve File from Bin\n";
    cout << "7.  Change File Expiry Time\n";
    cout << "8.  Search File\n";
    cout << "9.  List All Files\n";
    cout << "10. Delete File Permanently\n";
    cout << "11. Check Expired Files (Manual Check)\n";
    cout << "12. Show Disk Statistics\n";
    cout << "0.  Exit\n";
    printSeparator();
    cout << "Enter choice: ";
}

void checkExpiredFiles(FileManager& fm, MinHeap& heap, FileManagerDisk& disk) {
    lock_guard<mutex> heapLock(heapMutex);
    lock_guard<mutex> fmLock(fmMutex);
    
    time_t now = time(nullptr);
    
    while (!heap.isEmpty()) {
        HeapNode top = heap.peek();
        
        if (top.expireTime <= now) {
            cout << "\n[AUTO-EXPIRY] File ID " << top.fileId << " expired. Moving to bin...\n";
            
            // Move to bin (auto-saves to disk internally)
            if (fm.moveToBin(top.fileId)) {
                disk.updateFile(*fm.searchFile(top.fileId));
                cout << "[AUTO-EXPIRY] File " << top.fileId << " moved to bin.\n";
            }
            
            // Remove from heap
            heap.extractMin();
        } else {
            break; // No more expired files
        }
    }
}

// Background thread function
void expiryCheckerThread(FileManager& fm, MinHeap& heap, FileManagerDisk& disk) {
    while (keepRunning) {
        checkExpiredFiles(fm, heap, disk);
        this_thread::sleep_for(chrono::seconds(1)); // Check every second
    }
}

int main() {
    // Initialize components
    FileManager fm(10);
    FileManagerDisk disk("./disk.bin");
    MinHeap heap;
    
    cout << "\n===========================================\n";
    cout << "  TWO-TIER FILE MANAGEMENT SYSTEM\n";
    cout << "===========================================\n";
    cout << "Storage: Block-based disk (2GB, 50KB blocks)\n";
    
    // Load existing files from disk at startup
    cout << "\n[Startup] Loading files from disk...\n";
    disk.loadAllFiles(fm);
    
    // Rebuild heap with loaded files
    vector<FileEntry> activeFiles = fm.getActiveFiles();
    for (const auto& f : activeFiles) {
        heap.insert(f.fileId, f.expireTime);
    }
    cout << "[Startup] Ready!\n";
    
    // Start background expiry checker thread
    thread expiryThread(expiryCheckerThread, ref(fm), ref(heap), ref(disk));
    
    int choice;
    bool running = true;
    
    while (running) {
        displayMenu();
        cin >> choice;
        cin.ignore(); // Clear newline
        
        switch (choice) {
            case 1: { // Create File
                printSeparator();
                cout << "CREATE NEW FILE\n";
                printSeparator();
                
                string name, content;
                long expireSeconds;
                
                cout << "Enter file name: ";
                getline(cin, name);
                
                cout << "Enter content: ";
                getline(cin, content);
                
                cout << "Enter expiry time (seconds): ";
                cin >> expireSeconds;
                cin.ignore();
                
                int fileId = fm.createFile(name, content, expireSeconds);
                
                // Save to disk automatically happens inside createFile
                FileEntry* f = fm.searchFile(fileId);
                if (f) {
                    lock_guard<mutex> lock(heapMutex);
                    disk.saveFile(*f);
                    heap.insert(fileId, f->expireTime);
                    
                    cout << "\n✓ File created and saved to disk!\n";
                    cout << "  File ID: " << fileId << "\n";
                    cout << "  Name: " << name << "\n";
                    cout << "  Expires in: " << expireSeconds << " seconds\n";
                }
                break;
            }
            
            case 2: { // Write/Edit File
                printSeparator();
                cout << "WRITE/EDIT FILE\n";
                printSeparator();
                
                int fileId;
                string content;
                
                cout << "Enter file ID: ";
                cin >> fileId;
                cin.ignore();
                
                cout << "Enter new content: ";
                getline(cin, content);
                
                if (fm.writeFile(fileId, content)) {
                    // Auto-update on disk
                    FileEntry* f = fm.searchFile(fileId);
                    if (f) {
                        disk.updateFile(*f);
                        cout << "\n✓ File updated and saved to disk!\n";
                    }
                } else {
                    cout << "\n✗ Failed to write file. File may be in bin or doesn't exist.\n";
                }
                break;
            }
            
            case 3: { // Read File
                printSeparator();
                cout << "READ FILE\n";
                printSeparator();
                
                int fileId;
                string content;
                
                cout << "Enter file ID: ";
                cin >> fileId;
                cin.ignore();
                
                if (fm.readFile(fileId, content)) {
                    cout << "\n--- FILE CONTENT ---\n";
                    cout << content << "\n";
                    cout << "--------------------\n";
                } else {
                    cout << "\n✗ Cannot read file. File may be in bin or doesn't exist.\n";
                }
                break;
            }
            
            case 4: { // Truncate File
                printSeparator();
                cout << "TRUNCATE FILE\n";
                printSeparator();
                
                int fileId;
                cout << "Enter file ID: ";
                cin >> fileId;
                cin.ignore();
                
                if (fm.truncateFile(fileId)) {
                    FileEntry* f = fm.searchFile(fileId);
                    if (f) {
                        disk.updateFile(*f);
                        cout << "\n✓ File truncated and saved to disk!\n";
                    }
                } else {
                    cout << "\n✗ Failed to truncate file.\n";
                }
                break;
            }
            
            case 5: { // Move to Bin
                printSeparator();
                cout << "MOVE FILE TO BIN\n";
                printSeparator();
                
                int fileId;
                cout << "Enter file ID: ";
                cin >> fileId;
                cin.ignore();
                
                if (fm.moveToBin(fileId)) {
                    FileEntry* f = fm.searchFile(fileId);
                    if (f) {
                        lock_guard<mutex> lock(heapMutex);
                        disk.updateFile(*f);
                        heap.removeByFileId(fileId);
                        cout << "\n✓ File moved to bin and saved to disk!\n";
                    }
                } else {
                    cout << "\n✗ Failed to move file to bin.\n";
                }
                break;
            }
            
            case 6: { // Retrieve from Bin
                printSeparator();
                cout << "RETRIEVE FILE FROM BIN\n";
                printSeparator();
                
                int fileId;
                cout << "Enter file ID: ";
                cin >> fileId;
                cin.ignore();
                
                if (fm.retrieveFromBin(fileId)) {
                    FileEntry* f = fm.searchFile(fileId);
                    if (f) {
                        lock_guard<mutex> lock(heapMutex);
                        disk.updateFile(*f);
                        heap.insert(fileId, f->expireTime);
                        cout << "\n✓ File retrieved from bin and saved to disk!\n";
                        cout << "  Timer has been restarted.\n";
                    }
                } else {
                    cout << "\n✗ Failed to retrieve file from bin.\n";
                }
                break;
            }
            
            case 7: { // Change Expiry
                printSeparator();
                cout << "CHANGE FILE EXPIRY TIME\n";
                printSeparator();
                
                int fileId;
                long newExpireSeconds;
                
                cout << "Enter file ID: ";
                cin >> fileId;
                
                cout << "Enter new expiry time (seconds): ";
                cin >> newExpireSeconds;
                cin.ignore();
                
                if (fm.changeExpiry(fileId, newExpireSeconds)) {
                    FileEntry* f = fm.searchFile(fileId);
                    if (f) {
                        lock_guard<mutex> lock(heapMutex);
                        disk.updateFile(*f);
                        heap.removeByFileId(fileId);
                        heap.insert(fileId, f->expireTime);
                        cout << "\n✓ Expiry time updated and saved to disk!\n";
                    }
                } else {
                    cout << "\n✗ Failed to change expiry. File may be in bin.\n";
                }
                break;
            }
            
            case 8: { // Search File
                printSeparator();
                cout << "SEARCH FILE\n";
                printSeparator();
                
                int fileId;
                cout << "Enter file ID: ";
                cin >> fileId;
                cin.ignore();
                
                FileEntry* f = fm.searchFile(fileId);
                if (f) {
                    cout << "\n--- FILE FOUND ---\n";
                    cout << "File ID: " << f->fileId << "\n";
                    cout << "Name: " << f->name << "\n";
                    cout << "Content: " << f->content << "\n";
                    cout << "Created: " << ctime(&f->createTime);
                    cout << "Expires: " << ctime(&f->expireTime);
                    cout << "Status: " << (f->inBin ? "IN BIN" : "ACTIVE") << "\n";
                    cout << "------------------\n";
                    
                    if (f->inBin) {
                        cout << "\n⚠ File is in bin. Use option 6 to retrieve it.\n";
                    }
                } else {
                    cout << "\n✗ File not found.\n";
                }
                break;
            }
            
            case 9: { // List All Files
                printSeparator();
                cout << "ALL FILES\n";
                printSeparator();
                fm.listFiles();
                break;
            }
            
            case 10: { // Delete Permanently
                printSeparator();
                cout << "DELETE FILE PERMANENTLY\n";
                printSeparator();
                
                int fileId;
                cout << "Enter file ID: ";
                cin >> fileId;
                cin.ignore();
                
                FileEntry* f = fm.searchFile(fileId);
                if (f) {
                    cout << "⚠ WARNING: This will permanently delete the file from disk!\n";
                    cout << "Are you sure? (y/n): ";
                    char confirm;
                    cin >> confirm;
                    cin.ignore();
                    
                    if (confirm == 'y' || confirm == 'Y') {
                        // Delete from disk first
                        disk.deleteFile(fileId);
                        
                        // Remove from memory
                        {
                            lock_guard<mutex> lock(heapMutex);
                            if (f->inBin) {
                                // Just remove from bin
                            } else {
                                heap.removeByFileId(fileId);
                            }
                        }
                        
                        cout << "\n✓ File permanently deleted from disk and memory.\n";
                    } else {
                        cout << "\n✗ Deletion cancelled.\n";
                    }
                } else {
                    cout << "\n✗ File not found.\n";
                }
                break;
            }
            
            case 11: { // Check Expired Files
                printSeparator();
                cout << "CHECKING FOR EXPIRED FILES...\n";
                printSeparator();
                checkExpiredFiles(fm, heap, disk);
                cout << "\n✓ Expiry check complete.\n";
                break;
            }
            
            case 12: { // Show Disk Statistics
                printSeparator();
                disk.printDiskStats();
                break;
            }
            
            case 0: { // Exit
                printSeparator();
                cout << "EXITING...\n";
                printSeparator();
                
                cout << "\n✓ Stopping background expiry checker...\n";
                keepRunning = false;
                expiryThread.join(); // Wait for thread to finish
                
                cout << "✓ All changes saved to disk.\n";
                cout << "Goodbye!\n\n";
                running = false;
                break;
            }
            
            default:
                cout << "\n✗ Invalid choice. Please try again.\n";
                break;
        }
    }
    
    return 0;
}
