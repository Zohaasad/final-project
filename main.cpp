
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <sstream>
#include <vector>
#include <mutex>
#include <atomic>
#include "Queue.hpp"
#include "Protocol.hpp"
#include "FileManager.hpp"
#include "FileManagerDisk.hpp"
#include "MinHeap.hpp"
#include "UserManager.hpp"
#include "UserManagerDisk.hpp"

using namespace std;


FileManagerDisk* disk;
FileEntryHeap* heap;
UserManager* um;
Queue<int> clientQueue;
mutex queueMutex;
mutex heapMutex;
mutex diskMutex; 
atomic<bool> serverRunning(true);


void sendMessage(int clientSocket, const string& message) {
    send(clientSocket, message.c_str(), message.length(), 0);
}

string receiveMessage(int clientSocket) {
    char buffer[4096] = {0};
    int valread = read(clientSocket, buffer, 4096);
    if (valread <= 0) return "";
    return string(buffer);
}

vector<string> split(const string& str, const string& delimiter) {
    vector<string> tokens;
    size_t start = 0;
    size_t end = str.find(delimiter);
    while (end != string::npos) {
        tokens.push_back(str.substr(start, end - start));
        start = end + delimiter.length();
        end = str.find(delimiter, start);
    }
    tokens.push_back(str.substr(start));
    return tokens;
}

void checkExpiredFiles() {
   
    FileManager tempFm(-1);
    tempFm.setDiskManager(disk);
    
    lock_guard<mutex> heapLock(heapMutex);
    lock_guard<mutex> diskLock(diskMutex);
    
    time_t now = time(nullptr);

    while (!heap->isEmpty()) {
        FileEntry* top = heap->peek();
        if (!top) break;

        if (top->expireTime <= now) {
            cout << "[AUTO-EXPIRY] File ID " << top->fileId << " expired. Moving to bin...\n";
            
            
            top->inBin = true;
            top->inUse = false;
            disk->updateFile(*top);
            
            heap->extractMin();
            cout << "[AUTO-EXPIRY] File '" << top->name << "' moved to bin.\n";
        } else {
            break;
        }
    }
}

void expiryCheckerThread() {
    while (serverRunning) {
        checkExpiredFiles();
        this_thread::sleep_for(chrono::seconds(1));
    }
}


void handleClient(int clientSocket) {
    cout << "[SERVER] Client connected: " << clientSocket << endl;

  
    FileManager* fm = new FileManager(-1);
    fm->setDiskManager(disk);
    
   
    {
        lock_guard<mutex> diskLock(diskMutex);
        disk->loadAllFiles(*fm);
    }

    int currentUserId = -1;
    string currentUsername = "";
    bool sessionActive = true;

    while (sessionActive && serverRunning) {
        string request = receiveMessage(clientSocket);
        if (request.empty()) {
            cout << "[SERVER] Client disconnected: " << clientSocket << endl;
            break;
        }

        cout << "[SERVER] Received: " << request << endl;
        vector<string> parts = split(request, DELIMITER);
        if (parts.empty()) continue;

        string command = parts[0];
        string response;

        if (command == MSG_LOGIN) {
            if (parts.size() < 3) {
                response = RESP_FAILURE + DELIMITER + "Invalid login format";
            } else {
                string username = parts[1];
                string password = parts[2];
                int userId = um->loginUser(username, password);
                if (userId != -1) {
                    currentUserId = userId;
                    currentUsername = username;
                    fm->setCurrentUser(userId);  
                    response = RESP_SUCCESS + DELIMITER + "Login successful" + DELIMITER + username;
                } else {
                    response = RESP_FAILURE + DELIMITER + "Invalid username or password";
                }
            }
        }
        else if (command == MSG_REGISTER) {
            if (parts.size() < 3) {
                response = RESP_FAILURE + DELIMITER + "Invalid register format";
            } else {
                string username = parts[1];
                string password = parts[2];
                if (um->userExists(username)) {
                    response = RESP_FAILURE + DELIMITER + "Username already exists";
                } else if (um->registerUser(username, password)) {
                    response = RESP_SUCCESS + DELIMITER + "Registration successful";
                } else {
                    response = RESP_FAILURE + DELIMITER + "Registration failed";
                }
            }
        }
        else if (currentUserId == -1) {
            response = RESP_FAILURE + DELIMITER + "Not logged in";
        }
      
        else if (command == MSG_CREATE_FILE) {
            if (parts.size() < 4) {
                response = RESP_FAILURE + DELIMITER + "Invalid format";
            } else {
                string name = parts[1];
                string content = parts[2];
                long expireSeconds = stol(parts[3]);

                if (fm->createFile(name, content, expireSeconds)) {
                    FileEntry* f = fm->searchFile(name);
                    if (f) {
                        lock_guard<mutex> lock(heapMutex);
                        heap->push(f);
                    }
                    response = RESP_SUCCESS + DELIMITER + "File created successfully";
                } else {
                    response = RESP_FAILURE + DELIMITER + "File already exists";
                }
            }
        }
      
       else if (command == MSG_WRITE_FILE) {
    if (parts.size() < 3) {
        response = RESP_FAILURE + DELIMITER + "Invalid format";
    } else {
        string fileName = parts[1];
        string content = parts[2];
        
        FileEntry* f = fm->searchFile(fileName);
        if (!f) {
            response = RESP_FAILURE + DELIMITER + "File not found";
        } else if (f->inBin) {
            
            response = RESP_FAILURE + DELIMITER + "Cannot write: File is in bin. Please retrieve it first.";
        } else {
            if (fm->writeFile(fileName, content)) {
                response = RESP_SUCCESS + DELIMITER + "File updated successfully";
            } else {
                response = RESP_FAILURE + DELIMITER + "Failed to write file";
            }
        }
    }
}

     
        else if (command == MSG_READ_FILE) {
    if (parts.size() < 2) {
        response = RESP_FAILURE + DELIMITER + "Invalid format";
    } else {
        string fileName = parts[1];
        FileEntry* f = fm->searchFile(fileName);
        
        if (!f) {
            response = RESP_FAILURE + DELIMITER + "File not found";
        } else if (f->inBin) {
           
            response = RESP_FAILURE + DELIMITER + "Cannot read: File is in bin. Please retrieve it first.";
        } else {
            string content;
            if (fm->readFile(fileName, content)) {
                response = RESP_DATA + DELIMITER + fileName + DELIMITER + content;
            } else {
                response = RESP_FAILURE + DELIMITER + "Cannot read file";
            }
        }
    }
}
       
       else if (command == MSG_TRUNCATE_FILE) {
    if (parts.size() < 2) {
        response = RESP_FAILURE + DELIMITER + "Invalid format";
    } else {
        string fileName = parts[1];
        FileEntry* f = fm->searchFile(fileName);
        
        if (!f) {
            response = RESP_FAILURE + DELIMITER + "File not found";
        } else if (f->inBin) {
           
            response = RESP_FAILURE + DELIMITER + "Cannot truncate: File is in bin. Please retrieve it first.";
        } else {
            if (fm->truncateFile(fileName)) {
                response = RESP_SUCCESS + DELIMITER + "File truncated successfully";
            } else {
                response = RESP_FAILURE + DELIMITER + "Failed to truncate file";
            }
        }
    }
}

        
       else if (command == MSG_MOVE_TO_BIN) {
    if (parts.size() < 2) {
        response = RESP_FAILURE + DELIMITER + "Invalid format";
    } else {
        string fileName = parts[1];
        FileEntry* f = fm->searchFile(fileName);
        if (f) {
            if (f->inBin) {
                response = RESP_FAILURE + DELIMITER + "File already in bin";
            } else {
             
                {
                    lock_guard<mutex> lock(heapMutex);
                    heap->remove(f);
                }
                
            
                if (fm->moveToBin(fileName)) {
                    lock_guard<mutex> diskLock(diskMutex);
                    disk->updateFile(*f);
                    response = RESP_SUCCESS + DELIMITER + "File moved to bin and timer stopped";
                } else {
                    response = RESP_FAILURE + DELIMITER + "Failed to move file";
                }
            }
        } else {
            response = RESP_FAILURE + DELIMITER + "File not found";
        }
    }
}


else if (command == MSG_RETRIEVE_FROM_BIN) {
    if (parts.size() < 2) {
        response = RESP_FAILURE + DELIMITER + "Invalid format";
    } else {
        string fileName = parts[1];
        
     
        FileEntry* f = fm->searchFile(fileName);
        
        if (!f) {
            response = RESP_FAILURE + DELIMITER + "File not found";
        } else if (!f->inBin) {
            response = RESP_FAILURE + DELIMITER + "File is not in bin";
        } else {
            cout << "[DEBUG] Before retrieve: inBin=" << f->inBin << ", inUse=" << f->inUse << endl;
            
          
            if (fm->retrieveFromBin(fileName)) {
                
                cout << "[DEBUG] After retrieve: inBin=" << f->inBin << ", inUse=" << f->inUse << endl;
                
               
                {
                    lock_guard<mutex> lock(heapMutex);
                    heap->push(f);
                    cout << "[DEBUG] File added back to heap" << endl;
                }
                
              
                {
                    lock_guard<mutex> diskLock(diskMutex);
                    disk->updateFile(*f);
                    cout << "[DEBUG] File updated on disk" << endl;
                }
                
              
                vector<FileEntry> activeFiles = fm->getActiveFiles();
                bool foundInActive = false;
                for (auto& af : activeFiles) {
                    if (af.name == fileName) {
                        foundInActive = true;
                        break;
                    }
                }
                
                if (foundInActive) {
                    response = RESP_SUCCESS + DELIMITER + "File retrieved from bin successfully. Timer restarted. You can now edit, read, or truncate.";
                } else {
                    response = RESP_FAILURE + DELIMITER + "File retrieved but not showing in active files. Please check.";
                }
                
            } else {
                response = RESP_FAILURE + DELIMITER + "Failed to retrieve file from bin";
            }
        }
    }
}
        
       else if (command == MSG_CHANGE_EXPIRY) {
    if (parts.size() < 3) {
        response = RESP_FAILURE + DELIMITER + "Invalid format";
    } else {
        string fileName = parts[1];
        long newExpire = stol(parts[2]);
        
        FileEntry* f = fm->searchFile(fileName);
        if (!f) {
            response = RESP_FAILURE + DELIMITER + "File not found";
        } else if (f->inBin) {
            
            response = RESP_FAILURE + DELIMITER + "Cannot change expiry: File is in bin. Please retrieve it first.";
        } else {
            if (fm->changeExpiry(fileName, newExpire)) {
                {
                    lock_guard<mutex> lock(heapMutex);
                    heap->remove(f);
                    heap->push(f);
                }
                response = RESP_SUCCESS + DELIMITER + "Expiry time updated";
            } else {
                response = RESP_FAILURE + DELIMITER + "Failed to change expiry";
            }
        }
    }
}
      
        else if (command == MSG_SEARCH_FILE) {
            if (parts.size() < 2) {
                response = RESP_FAILURE + DELIMITER + "Invalid format";
            } else {
                string fileName = parts[1];
                FileEntry* f = fm->searchFile(fileName);
                if (f) {
                    stringstream ss;
                    ss << RESP_DATA << DELIMITER
                       << f->name << DELIMITER
                       << f->content << DELIMITER
                       << f->createTime << DELIMITER
                       << f->expireTime << DELIMITER
                       << (f->inBin ? "1" : "0");
                    response = ss.str();
                } else {
                    response = RESP_FAILURE + DELIMITER + "File not found";
                }
            }
        }
   
        else if (command == MSG_LIST_FILES) {
            stringstream ss;
            ss << RESP_DATA << DELIMITER;
            vector<FileEntry> activeFiles = fm->getActiveFiles();
            ss << activeFiles.size() << DELIMITER;
            for (auto& f : activeFiles) {
                ss << f.name << DELIMITER
                   << f.content << DELIMITER
                   << f.createTime << DELIMITER
                   << f.expireTime << DELIMITER;
            }
            vector<FileEntry> binFiles = fm->getBinFiles();
            ss << binFiles.size() << DELIMITER;
            for (auto& f : binFiles) {
                ss << f.name << DELIMITER
                   << f.createTime << DELIMITER
                   << f.expireTime << DELIMITER;
            }
            response = ss.str();
        }
    
        else if (command == MSG_DELETE_PERMANENTLY) {
            if (parts.size() < 2) {
                response = RESP_FAILURE + DELIMITER + "Invalid format";
            } else {
                string fileName = parts[1];
                FileEntry* f = fm->searchFile(fileName);
                if (f) {
                    bool inBin = f->inBin;
                    int fileId = f->fileId;
                    
                    if (fm->removeFileCompletely(fileName)) {
                        if (!inBin) {
                            lock_guard<mutex> lock(heapMutex);
                            heap->remove(f);
                        }
                        
                        lock_guard<mutex> diskLock(diskMutex);
                        disk->deleteFile(fileId);
                        
                        response = RESP_SUCCESS + DELIMITER + "File permanently deleted";
                    } else {
                        response = RESP_FAILURE + DELIMITER + "Failed to delete file";
                    }
                } else {
                    response = RESP_FAILURE + DELIMITER + "File not found";
                }
            }
        }
      
        else if (command == MSG_DISK_STATS) {
            lock_guard<mutex> diskLock(diskMutex);
            int used = disk->getUsedBlocks();
            int free = disk->getFreeBlocks();
            float usedMB = (used * 50 * 1024) / (1024.0 * 1024.0);
            float freeMB = (free * 50 * 1024) / (1024.0 * 1024.0);
            float usage = (used * 100.0) / (used + free);
            stringstream ss;
            ss << RESP_DATA << DELIMITER
               << (used + free) << DELIMITER
               << "50" << DELIMITER
               << used << DELIMITER
               << usedMB << DELIMITER
               << free << DELIMITER
               << freeMB << DELIMITER
               << usage;
            response = ss.str();
        }
    
        else if (command == MSG_LOGOUT) {
            response = RESP_SUCCESS + DELIMITER + "Logged out successfully";
            currentUserId = -1;
            currentUsername = "";
            fm->setCurrentUser(-1);
        }
      
        else if (command == MSG_EXIT) {
            response = RESP_SUCCESS + DELIMITER + "Goodbye";
            sessionActive = false;
        }
        else {
            response = RESP_FAILURE + DELIMITER + "Unknown command";
        }

        sendMessage(clientSocket, response);
        cout << "[SERVER] Sent: " << response << endl;
    }

   
    delete fm;
    
    close(clientSocket);
    cout << "[SERVER] Client session ended: " << clientSocket << endl;
}

void workerThread() {
    while (serverRunning) {
        int clientSocket = -1;
        {
            lock_guard<mutex> lock(queueMutex);
            if (!clientQueue.isEmpty()) {
                clientSocket = clientQueue.dequeue();
            }
        }
        if (clientSocket != -1) {
            handleClient(clientSocket);
        } else {
            this_thread::sleep_for(chrono::milliseconds(100));
        }
    }
}

int main() {
    disk = new FileManagerDisk("./disk.bin");
    heap = new FileEntryHeap();
    um = new UserManager();
    UserManagerDisk* userDisk = new UserManagerDisk("./users.dat");
    um->setDiskManager(userDisk);

    cout << "\n===========================================\n";
    cout << "  FILE MANAGEMENT SERVER\n";
    cout << "===========================================\n";
    userDisk->loadAllUsers(*um);
    cout << "[System] Rebuilding expiry heap from disk...\n";
    FileManager tempFm(-1);
    tempFm.setDiskManager(disk);
    disk->loadAllFiles(tempFm);
    vector<FileEntry> allFiles = tempFm.getActiveFiles();
    int heapCount = 0;
    for (auto& f : allFiles) {
        if (!f.inBin) {
            FileEntry* ptr = tempFm.searchFile(f.name);
            if (ptr) {
                heap->push(ptr);
                heapCount++;
            }
        }
    }
    cout << "[System] Heap restored with " << heapCount << " active files\n";
    thread expiryThread(expiryCheckerThread);
    const int NUM_WORKERS = 5;
    vector<thread> workers;
    for (int i = 0; i < NUM_WORKERS; i++) {
        workers.emplace_back(workerThread);
    }
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) { cerr << "[ERROR] Failed to create socket\n"; return 1; }
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(8080);
    if (::bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        cerr << "[ERROR] Bind failed\n"; close(serverSocket); return 1;
    }
    if (listen(serverSocket, 10) < 0) {
        cerr << "[ERROR] Listen failed\n"; close(serverSocket); return 1;
    }

    cout << "[SERVER] Listening on port 8080...\n";
    while (serverRunning) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientSocket < 0) {
            if (serverRunning) cerr << "[ERROR] Accept failed\n";
            continue;
        }

        cout << "[SERVER] New connection from " << inet_ntoa(clientAddr.sin_addr)
             << ":" << ntohs(clientAddr.sin_port) << endl;

        lock_guard<mutex> lock(queueMutex);
        clientQueue.enqueue(clientSocket);
    }
    cout << "\n[SERVER] Shutting down...\n";
    serverRunning = false;
    expiryThread.join();
    for (auto& worker : workers) worker.join();
    close(serverSocket);
    delete disk;
    delete heap;
    delete um;
    delete userDisk;
    cout << "[SERVER] Server stopped\n";
    return 0;
}
