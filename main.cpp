
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
#include <set>
#include "Queue.hpp"
#include "Protocol.hpp"
#include "FileManager.hpp"
#include "FileManagerDisk.hpp"
#include "MinHeap.hpp"
#include "UserManager.hpp"
#include "UserManagerDisk.hpp"

using namespace std;
FileManagerDisk* disk;
UserManager* um;
FileManager* globalFm;  
set<int> loggedInUsers;
mutex loggedInUsersMutex;

Queue<int> clientQueue;
mutex queueMutex;
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
    lock_guard<mutex> diskLock(diskMutex);
    
    globalFm->updateExpiryStatus();
}

void expiryCheckerThread() {
    while (serverRunning) {
        checkExpiredFiles();
        this_thread::sleep_for(chrono::seconds(1));
    }
}

void handleClient(int clientSocket) {
    cout << "[SERVER] Client connected: " << clientSocket << endl;

    int currentUserId = -1;
    string currentUsername = "";
    bool sessionActive = true;

    while (sessionActive && serverRunning) {
        string request = receiveMessage(clientSocket);
        if (request.empty()) {
            cout << "[SERVER] Client disconnected: " << clientSocket << endl;
            if (currentUserId != -1) {
                lock_guard<mutex> diskLock(diskMutex);
                lock_guard<mutex> usersLock(loggedInUsersMutex);
                
                globalFm->unloadUserFiles(currentUserId);
                loggedInUsers.erase(currentUserId);
                cout << "[SERVER] Auto-logged out user " << currentUserId << " on disconnect\n";
            }
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
                    
                    lock_guard<mutex> diskLock(diskMutex);
                    lock_guard<mutex> usersLock(loggedInUsersMutex);
                    
                    globalFm->setCurrentUser(userId);
                    globalFm->loadUserFiles(userId); 
                    loggedInUsers.insert(userId);
                    
                    response = RESP_SUCCESS + DELIMITER + "Login successful" + DELIMITER + username;
                    cout << "[SERVER] User " << userId << " logged in. Files loaded into memory.\n";
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

                lock_guard<mutex> diskLock(diskMutex);
                if (globalFm->createFile(name, content, expireSeconds)) {
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
                
                lock_guard<mutex> diskLock(diskMutex);
                FileEntry* f = globalFm->searchFile(fileName);
                if (!f) {
                    response = RESP_FAILURE + DELIMITER + "File not found";
                } else if (f->inBin) {
                    response = RESP_FAILURE + DELIMITER + "Cannot write: File is in bin. Please retrieve it first.";
                } else {
                    if (globalFm->writeFile(fileName, content)) {
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
                
                lock_guard<mutex> diskLock(diskMutex);
                FileEntry* f = globalFm->searchFile(fileName);
                
                if (!f) {
                    response = RESP_FAILURE + DELIMITER + "File not found";
                } else if (f->inBin) {
                    response = RESP_FAILURE + DELIMITER + "Cannot read: File is in bin. Please retrieve it first.";
                } else {
                    string content;
                    if (globalFm->readFile(fileName, content)) {
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
                
                lock_guard<mutex> diskLock(diskMutex);
                FileEntry* f = globalFm->searchFile(fileName);
                
                if (!f) {
                    response = RESP_FAILURE + DELIMITER + "File not found";
                } else if (f->inBin) {
                    response = RESP_FAILURE + DELIMITER + "Cannot truncate: File is in bin. Please retrieve it first.";
                } else {
                    if (globalFm->truncateFile(fileName)) {
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
                
                lock_guard<mutex> diskLock(diskMutex);
                FileEntry* f = globalFm->searchFile(fileName);
                if (f) {
                    if (f->inBin) {
                        response = RESP_FAILURE + DELIMITER + "File already in bin";
                    } else {
                        if (globalFm->moveToBin(fileName)) {
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
                
                lock_guard<mutex> diskLock(diskMutex);
                FileEntry* f = globalFm->searchFile(fileName);
                
                if (!f) {
                    response = RESP_FAILURE + DELIMITER + "File not found";
                } else if (!f->inBin) {
                    response = RESP_FAILURE + DELIMITER + "File is not in bin";
                } else {
                    if (globalFm->retrieveFromBin(fileName)) {
                        response = RESP_SUCCESS + DELIMITER + "File retrieved from bin successfully. Timer restarted. You can now edit, read, or truncate.";
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
                
                lock_guard<mutex> diskLock(diskMutex);
                FileEntry* f = globalFm->searchFile(fileName);
                if (!f) {
                    response = RESP_FAILURE + DELIMITER + "File not found";
                } else if (f->inBin) {
                    response = RESP_FAILURE + DELIMITER + "Cannot change expiry: File is in bin. Please retrieve it first.";
                } else {
                    if (globalFm->changeExpiry(fileName, newExpire)) {
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
                
                lock_guard<mutex> diskLock(diskMutex);
                FileEntry* f = globalFm->searchFile(fileName);
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
            lock_guard<mutex> diskLock(diskMutex);
            
            stringstream ss;
            ss << RESP_DATA << DELIMITER;
            vector<FileEntry> activeFiles = globalFm->getActiveFiles();
            ss << activeFiles.size() << DELIMITER;
            for (auto& f : activeFiles) {
                ss << f.name << DELIMITER
                   << f.content << DELIMITER
                   << f.createTime << DELIMITER
                   << f.expireTime << DELIMITER;
            }
            vector<FileEntry> binFiles = globalFm->getBinFiles();
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
                
                lock_guard<mutex> diskLock(diskMutex);
                if (globalFm->removeFileCompletely(fileName)) {
                    response = RESP_SUCCESS + DELIMITER + "File permanently deleted";
                } else {
                    response = RESP_FAILURE + DELIMITER + "Failed to delete file";
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
            lock_guard<mutex> diskLock(diskMutex);
            lock_guard<mutex> usersLock(loggedInUsersMutex);
            
            globalFm->unloadUserFiles(currentUserId);
            loggedInUsers.erase(currentUserId);
            
            cout << "[SERVER] User " << currentUserId << " logged out. Files unloaded from memory.\n";
            
            response = RESP_SUCCESS + DELIMITER + "Logged out successfully";
            currentUserId = -1;
            currentUsername = "";
            globalFm->setCurrentUser(-1);
        }
        else if (command == MSG_EXIT) {
            
            if (currentUserId != -1) {
                lock_guard<mutex> diskLock(diskMutex);
                lock_guard<mutex> usersLock(loggedInUsersMutex);
                globalFm->unloadUserFiles(currentUserId);
                loggedInUsers.erase(currentUserId);
            }
            
            response = RESP_SUCCESS + DELIMITER + "Goodbye";
            sessionActive = false;
        }
        else {
            response = RESP_FAILURE + DELIMITER + "Unknown command";
        }

        sendMessage(clientSocket, response);
        cout << "[SERVER] Sent: " << response << endl;
    }

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
    um = new UserManager();
    UserManagerDisk* userDisk = new UserManagerDisk("./users.dat");
    um->setDiskManager(userDisk);

    cout << "\n===========================================\n";
    cout << "  FILE MANAGEMENT SERVER (On-Demand Loading)\n";
    cout << "===========================================\n";
    
    userDisk->loadAllUsers(*um);
    
    globalFm = new FileManager(-1);
    globalFm->setDiskManager(disk);
    
    cout << "[System] File manager initialized (files will load on login).\n";
    cout << "[System] Memory usage: 0 MB (no files loaded yet).\n";

    thread expiryThread(expiryCheckerThread);

 
    const int NUM_WORKERS = 5;
    vector<thread> workers;
    for (int i = 0; i < NUM_WORKERS; i++) {
        workers.emplace_back(workerThread);
    }


    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) { 
        cerr << "[ERROR] Failed to create socket\n"; 
        return 1; 
    }
    
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(8080);
    
    if (::bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        cerr << "[ERROR] Bind failed\n"; 
        close(serverSocket); 
        return 1;
    }
    
    if (listen(serverSocket, 10) < 0) {
        cerr << "[ERROR] Listen failed\n"; 
        close(serverSocket); 
        return 1;
    }

    cout << "[SERVER] Listening on port 8080...\n";
    cout << "[SERVER] Files will be loaded from disk when users log in.\n";
    
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
    
    delete globalFm;
    delete disk;
    delete um;
    delete userDisk;
    
    cout << "[SERVER] Server stopped\n";
    return 0;
}
