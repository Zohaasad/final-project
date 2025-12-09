#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <string>

// Message Types
const std::string MSG_LOGIN = "LOGIN";
const std::string MSG_REGISTER = "REGISTER";
const std::string MSG_CREATE_FILE = "CREATE_FILE";
const std::string MSG_WRITE_FILE = "WRITE_FILE";
const std::string MSG_READ_FILE = "READ_FILE";
const std::string MSG_TRUNCATE_FILE = "TRUNCATE_FILE";
const std::string MSG_MOVE_TO_BIN = "MOVE_TO_BIN";
const std::string MSG_RETRIEVE_FROM_BIN = "RETRIEVE_FROM_BIN";
const std::string MSG_CHANGE_EXPIRY = "CHANGE_EXPIRY";
const std::string MSG_SEARCH_FILE = "SEARCH_FILE";
const std::string MSG_LIST_FILES = "LIST_FILES";
const std::string MSG_DELETE_PERMANENTLY = "DELETE_PERMANENTLY";
const std::string MSG_CHECK_EXPIRED = "CHECK_EXPIRED";
const std::string MSG_DISK_STATS = "DISK_STATS";
const std::string MSG_LOGOUT = "LOGOUT";
const std::string MSG_EXIT = "EXIT";

// Response codes
const std::string RESP_SUCCESS = "SUCCESS";
const std::string RESP_FAILURE = "FAILURE";
const std::string RESP_DATA = "DATA";

// Delimiter
const std::string DELIMITER = "|";

#endif // PROTOCOL_HPP
