#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <vector>
#include <ctime>
#include "Protocol.hpp"

using namespace std;

int clientSocket = -1;
string currentUsername = "";
bool isLoggedIn = false;

// Helper function to send message
void sendMessage(const string& message) {
    send(clientSocket, message.c_str(), message.length(), 0);
}

// Helper function to receive message
string receiveMessage() {
    char buffer[8192] = {0};
    int valread = read(clientSocket, buffer, 8192);
    if (valread <= 0) return "";
    return string(buffer);
}

// Split string by delimiter
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

void printSeparator() {
    cout << "\n========================================\n";
}

void displayMainMenu() {
    printSeparator();
    cout << "WELCOME TO FILE MANAGEMENT SYSTEM\n";
    printSeparator();
    cout << "1. Login\n";
    cout << "2. Register New User\n";
    cout << "0. Exit\n";
    printSeparator();
    cout << "Enter choice: ";
}

void displayFileMenu() {
    printSeparator();
    cout << "FILE MANAGEMENT - User: " << currentUsername << "\n";
    printSeparator();
    cout << "1.  Create File\n";
    cout << "2.  Write/Edit File\n";
    cout << "3.  Read File\n";
    cout << "4.  Truncate File\n";
    cout << "5.  Move File to Bin\n";
    cout << "6.  Retrieve File from Bin\n";
    cout << "7.  Change File Expiry Time\n";
    cout << "8.  Search File\n";
    cout << "9.  List All My Files\n";
    cout << "10. Delete File Permanently\n";
   
    cout << "11. Show Disk Statistics\n";
    cout << "0.  Logout\n";
    printSeparator();
    cout << "Enter choice: ";
}

bool connectToServer(const string& serverIP, int port) {
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        cerr << "[ERROR] Failed to create socket\n";
        return false;
    }
    
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr) <= 0) {
        cerr << "[ERROR] Invalid address\n";
        close(clientSocket);
        return false;
    }
    
    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        cerr << "[ERROR] Connection failed\n";
        close(clientSocket);
        return false;
    }
    
    cout << "[CLIENT] Connected to server at " << serverIP << ":" << port << endl;
    return true;
}

void handleLogin() {
    printSeparator();
    cout << "LOGIN\n";
    printSeparator();
    
    string username, password;
    
    cout << "Username: ";
    getline(cin, username);
    
    cout << "Password: ";
    getline(cin, password);
    
    string request = MSG_LOGIN + DELIMITER + username + DELIMITER + password;
    sendMessage(request);
    
    string response = receiveMessage();
    vector<string> parts = split(response, DELIMITER);
    
    if (!parts.empty() && parts[0] == RESP_SUCCESS) {
        cout << "\n✓ Login successful! Welcome, " << username << "!\n";
        isLoggedIn = true;
        currentUsername = username;
    } else {
        cout << "\n✗ Login failed. ";
        if (parts.size() > 1) {
            cout << parts[1];
        }
        cout << "\n";
    }
}

void handleRegister() {
    printSeparator();
    cout << "REGISTER NEW USER\n";
    printSeparator();
    
    string username, password;
    
    cout << "Choose username: ";
    getline(cin, username);
    
    cout << "Choose password: ";
    getline(cin, password);
    
    string request = MSG_REGISTER + DELIMITER + username + DELIMITER + password;
    sendMessage(request);
    
    string response = receiveMessage();
    vector<string> parts = split(response, DELIMITER);
    
    if (!parts.empty() && parts[0] == RESP_SUCCESS) {
        cout << "\n✓ User '" << username << "' registered successfully!\n";
        cout << "You can now login.\n";
    } else {
        cout << "\n✗ Registration failed. ";
        if (parts.size() > 1) {
            cout << parts[1];
        }
        cout << "\n";
    }
}

void handleCreateFile() {
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
    
    string request = MSG_CREATE_FILE + DELIMITER + name + DELIMITER + content + DELIMITER + to_string(expireSeconds);
    sendMessage(request);
    
    string response = receiveMessage();
    vector<string> parts = split(response, DELIMITER);
    
    if (!parts.empty() && parts[0] == RESP_SUCCESS) {
        cout << "\n✓ File '" << name << "' created successfully!\n";
        cout << "  Expires in: " << expireSeconds << " seconds\n";
    } else {
        cout << "\n✗ Failed to create file. ";
        if (parts.size() > 1) {
            cout << parts[1];
        }
        cout << "\n";
    }
}
void handleWriteFile() {
    printSeparator();
    cout << "WRITE/EDIT FILE\n";
    printSeparator();
    
    string fileName, content;
    
    cout << "Enter file name: ";
    getline(cin, fileName);
    
    // First read the file to show current content
    string readRequest = MSG_READ_FILE + DELIMITER + fileName;
    sendMessage(readRequest);
    string readResponse = receiveMessage();
    vector<string> readParts = split(readResponse, DELIMITER);
    
    if (!readParts.empty() && readParts[0] == RESP_DATA && readParts.size() >= 3) {
        string currentContent = readParts[2];
        cout << "\n--- CURRENT CONTENT ---\n";
        cout << currentContent << "\n";
        cout << "-----------------------\n\n";
        
        cout << "Choose an option:\n";
        cout << "1. Replace content completely\n";
        cout << "2. Append to existing content\n";
        cout << "Enter choice (1 or 2): ";
        int choice;
        cin >> choice;
        cin.ignore();
        
        if (choice == 1) {
            cout << "\nEnter new content: ";
            getline(cin, content);
        } else if (choice == 2) {
            cout << "\nEnter text to append: ";
            string appendText;
            getline(cin, appendText);
            content = currentContent + appendText;
        } else {
            cout << "\n✗ Invalid choice. Operation cancelled.\n";
            return;
        }
    } else {
        cout << "\nFile not found or cannot read. Enter new content: ";
        getline(cin, content);
    }
    
    string request = MSG_WRITE_FILE + DELIMITER + fileName + DELIMITER + content;
    sendMessage(request);
    
    string response = receiveMessage();
    vector<string> parts = split(response, DELIMITER);
    
    if (!parts.empty() && parts[0] == RESP_SUCCESS) {
        cout << "\n✓ File '" << fileName << "' updated successfully!\n";
    } else {
        cout << "\n✗ Failed to write file. ";
        if (parts.size() > 1) {
            cout << parts[1];
        }
        cout << "\n";
    }
}
void handleReadFile() {
    printSeparator();
    cout << "READ FILE\n";
    printSeparator();
    
    string fileName;
    
    cout << "Enter file name: ";
    getline(cin, fileName);
    
    string request = MSG_READ_FILE + DELIMITER + fileName;
    sendMessage(request);
    
    string response = receiveMessage();
    vector<string> parts = split(response, DELIMITER);
    
    if (!parts.empty() && parts[0] == RESP_DATA && parts.size() >= 3) {
        cout << "\n--- FILE: " << parts[1] << " ---\n";
        cout << parts[2] << "\n";
        cout << "--------------------\n";
    } else {
        cout << "\n✗ Cannot read file. ";
        if (parts.size() > 1) {
            cout << parts[1];
        }
        cout << "\n";
    }
}

void handleTruncateFile() {
    printSeparator();
    cout << "TRUNCATE FILE\n";
    printSeparator();
    
    string fileName;
    cout << "Enter file name: ";
    getline(cin, fileName);
    
    string request = MSG_TRUNCATE_FILE + DELIMITER + fileName;
    sendMessage(request);
    
    string response = receiveMessage();
    vector<string> parts = split(response, DELIMITER);
    
    if (!parts.empty() && parts[0] == RESP_SUCCESS) {
        cout << "\n✓ File '" << fileName << "' truncated successfully!\n";
    } else {
        cout << "\n✗ Failed to truncate file. ";
        if (parts.size() > 1) {
            cout << parts[1];
        }
        cout << "\n";
    }
}

void handleMoveToBin() {
    printSeparator();
    cout << "MOVE FILE TO BIN\n";
    printSeparator();
    
    string fileName;
    cout << "Enter file name: ";
    getline(cin, fileName);
    
    string request = MSG_MOVE_TO_BIN + DELIMITER + fileName;
    sendMessage(request);
    
    string response = receiveMessage();
    vector<string> parts = split(response, DELIMITER);
    
    if (!parts.empty() && parts[0] == RESP_SUCCESS) {
        cout << "\n✓ File '" << fileName << "' moved to bin!\n";
    } else {
        cout << "\n✗ Failed to move file. ";
        if (parts.size() > 1) {
            cout << parts[1];
        }
        cout << "\n";
    }
}

void handleRetrieveFromBin() {
    printSeparator();
    cout << "RETRIEVE FILE FROM BIN\n";
    printSeparator();
    
    string fileName;
    cout << "Enter file name: ";
    getline(cin, fileName);
    
    string request = MSG_RETRIEVE_FROM_BIN + DELIMITER + fileName;
    sendMessage(request);
    
    string response = receiveMessage();
    vector<string> parts = split(response, DELIMITER);
    
    if (!parts.empty() && parts[0] == RESP_SUCCESS) {
        cout << "\n✓ File '" << fileName << "' retrieved from bin!\n";
        cout << "  Timer has been restarted.\n";
    } else {
        cout << "\n✗ Failed to retrieve file. ";
        if (parts.size() > 1) {
            cout << parts[1];
        }
        cout << "\n";
    }
}

void handleChangeExpiry() {
    printSeparator();
    cout << "CHANGE FILE EXPIRY TIME\n";
    printSeparator();
    
    string fileName;
    long newExpireSeconds;
    
    cout << "Enter file name: ";
    getline(cin, fileName);
    
    cout << "Enter new expiry time (seconds): ";
    cin >> newExpireSeconds;
    cin.ignore();
    
    string request = MSG_CHANGE_EXPIRY + DELIMITER + fileName + DELIMITER + to_string(newExpireSeconds);
    sendMessage(request);
    
    string response = receiveMessage();
    vector<string> parts = split(response, DELIMITER);
    
    if (!parts.empty() && parts[0] == RESP_SUCCESS) {
        cout << "\n✓ Expiry time updated for '" << fileName << "'!\n";
    } else {
        cout << "\n✗ Failed to change expiry. ";
        if (parts.size() > 1) {
            cout << parts[1];
        }
        cout << "\n";
    }
}

void handleSearchFile() {
    printSeparator();
    cout << "SEARCH FILE\n";
    printSeparator();
    
    string fileName;
    cout << "Enter file name: ";
    getline(cin, fileName);
    
    string request = MSG_SEARCH_FILE + DELIMITER + fileName;
    sendMessage(request);
    
    string response = receiveMessage();
    vector<string> parts = split(response, DELIMITER);
    
    if (!parts.empty() && parts[0] == RESP_DATA && parts.size() >= 6) {
        cout << "\n--- FILE FOUND ---\n";
        cout << "File Name: " << parts[1] << "\n";
        cout << "Content: " << parts[2] << "\n";
        
        time_t createTime = stol(parts[3]);
        time_t expireTime = stol(parts[4]);
        bool inBin = (parts[5] == "1");
        
        cout << "Created: " << ctime(&createTime);
        cout << "Expires: " << ctime(&expireTime);
        cout << "Status: " << (inBin ? "IN BIN" : "ACTIVE") << "\n";
        cout << "------------------\n";
        
        if (inBin) {
            cout << "\n⚠ File is in bin. Use option 6 to retrieve it.\n";
        }
    } else {
        cout << "\n✗ File not found. ";
        if (parts.size() > 1) {
            cout << parts[1];
        }
        cout << "\n";
    }
}

void handleListFiles() {
    printSeparator();
    
    string request = MSG_LIST_FILES;
    sendMessage(request);
    
    string response = receiveMessage();
    vector<string> parts = split(response, DELIMITER);
    
    if (!parts.empty() && parts[0] == RESP_DATA) {
        int idx = 1;
        
        // Active files
        cout << "\n========== YOUR ACTIVE FILES ==========\n";
        if (idx < parts.size()) {
            int activeCount = stoi(parts[idx++]);
            
            if (activeCount == 0) {
                cout << "No active files.\n";
            } else {
                for (int i = 0; i < activeCount && idx + 3 < parts.size(); i++) {
                    string name = parts[idx++];
                    string content = parts[idx++];
                    time_t createTime = stol(parts[idx++]);
                    time_t expireTime = stol(parts[idx++]);
                    
                    cout << "File: " << name << "\n";
                    cout << "  Content: " << content << "\n";
                    cout << "  Created: " << ctime(&createTime);
                    cout << "  Expires: " << ctime(&expireTime);
                }
            }
        }
        
        // Bin files
        cout << "\n========== YOUR FILES IN BIN ==========\n";
        if (idx < parts.size()) {
            int binCount = stoi(parts[idx++]);
            
            if (binCount == 0) {
                cout << "No files in bin.\n";
            } else {
                for (int i = 0; i < binCount && idx + 2 < parts.size(); i++) {
                    string name = parts[idx++];
                    time_t createTime = stol(parts[idx++]);
                    time_t expireTime = stol(parts[idx++]);
                    
                    cout << "File: " << name << "\n";
                    cout << "  Created: " << ctime(&createTime);
                    cout << "  Expires: " << ctime(&expireTime);
                }
            }
        }
        cout << "=======================================\n\n";
    } else {
        cout << "Failed to retrieve file list.\n";
    }
}

void handleDeletePermanently() {
    printSeparator();
    cout << "DELETE FILE PERMANENTLY\n";
    printSeparator();
    
    string fileName;
    cout << "Enter file name: ";
    getline(cin, fileName);
    
    cout << "⚠ WARNING: This will permanently delete '" << fileName << "' from disk!\n";
    cout << "Are you sure? (y/n): ";
    char confirm;
    cin >> confirm;
    cin.ignore();
    
    if (confirm == 'y' || confirm == 'Y') {
        string request = MSG_DELETE_PERMANENTLY + DELIMITER + fileName;
        sendMessage(request);
        
        string response = receiveMessage();
        vector<string> parts = split(response, DELIMITER);
        
        if (!parts.empty() && parts[0] == RESP_SUCCESS) {
            cout << "\n✓ File '" << fileName << "' permanently deleted.\n";
        } else {
            cout << "\n✗ Failed to delete file. ";
            if (parts.size() > 1) {
                cout << parts[1];
            }
            cout << "\n";
        }
    } else {
        cout << "\n✗ Deletion cancelled.\n";
    }
}

void handleCheckExpired() {
    printSeparator();
    cout << "CHECKING FOR EXPIRED FILES...\n";
    printSeparator();
    
    string request = MSG_CHECK_EXPIRED;
    sendMessage(request);
    
    string response = receiveMessage();
    vector<string> parts = split(response, DELIMITER);
    
    if (!parts.empty() && parts[0] == RESP_SUCCESS) {
        cout << "\n✓ Expiry check complete.\n";
    } else {
        cout << "\n✗ Failed to check expired files.\n";
    }
}

void handleDiskStats() {
    printSeparator();
    
    string request = MSG_DISK_STATS;
    sendMessage(request);
    
    string response = receiveMessage();
    vector<string> parts = split(response, DELIMITER);
    
    if (!parts.empty() && parts[0] == RESP_DATA && parts.size() >= 8) {
        cout << "\n========== DISK STATISTICS ==========\n";
        cout << "Total Blocks: " << parts[1] << "\n";
        cout << "Block Size: " << parts[2] << " KB\n";
        cout << "Used Blocks: " << parts[3] << " (" << parts[4] << " MB)\n";
        cout << "Free Blocks: " << parts[5] << " (" << parts[6] << " MB)\n";
        cout << "Disk Usage: " << parts[7] << "%\n";
        cout << "=====================================\n\n";
    } else {
        cout << "Failed to retrieve disk statistics.\n";
    }
}

void handleLogout() {
    string request = MSG_LOGOUT;
    sendMessage(request);
    
    string response = receiveMessage();
    
    cout << "\n✓ Logging out...\n";
    isLoggedIn = false;
    currentUsername = "";
}

void fileManagementSession() {
    int choice;
    
    while (isLoggedIn) {
        displayFileMenu();
        cin >> choice;
        cin.ignore();
        
        switch (choice) {
            case 1:
                handleCreateFile();
                break;
            case 2:
                handleWriteFile();
                break;
            case 3:
                handleReadFile();
                break;
            case 4:
                handleTruncateFile();
                break;
            case 5:
                handleMoveToBin();
                break;
            case 6:
                handleRetrieveFromBin();
                break;
            case 7:
                handleChangeExpiry();
                break;
            case 8:
                handleSearchFile();
                break;
            case 9:
                handleListFiles();
                break;
            case 10:
                handleDeletePermanently();
                break;
            
            case 11:
                handleDiskStats();
                break;
            case 0:
                handleLogout();
                break;
            default:
                cout << "\n✗ Invalid choice. Please try again.\n";
                break;
        }
    }
}

int main() {
    cout << "\n===========================================\n";
    cout << "  FILE MANAGEMENT CLIENT\n";
    cout << "===========================================\n\n";
    
    // Connect to server
    string serverIP = "127.0.0.1";  // Change this to server IP if needed
    int port = 8080;
    
    if (!connectToServer(serverIP, port)) {
        return 1;
    }
    
    bool running = true;
    
    while (running) {
        displayMainMenu();
        int choice;
        cin >> choice;
        cin.ignore();
        
        switch (choice) {
            case 1:
                handleLogin();
                if (isLoggedIn) {
                    fileManagementSession();
                }
                break;
            case 2:
                handleRegister();
                break;
            case 0:{
                cout << "\n✓ Exiting...\n";
                string request = MSG_EXIT;
                sendMessage(request);
                receiveMessage();
                running = false;
                break;}
            default:
                cout << "\n✗ Invalid choice. Please try again.\n";
                break;
        }
    }
    
    close(clientSocket);
    cout << "Goodbye!\n\n";
    
    return 0;
}
