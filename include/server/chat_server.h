#pragma once
#include <vector>
#include <mutex>
#include <string>
extern std::vector<int> clients;
extern std::mutex clients_mutex;
void handle_client(int client_fd);
void broadcast_msg(int sender_fd, const std::string& msg);
void remove_client(int client_fd);