#pragma once
#include <vector>
#include <mutex>
#include <string>
struct ClientInfo{
    int sock;
    std::string name;
    bool registered;
   };
extern std::vector<ClientInfo> clients;
extern std::mutex clients_mutex;
void handle_client(int client_fd);
void broadcast_msg(int sender_fd, const std::string& msg);
void remove_client(int client_fd);
std::string get_client_name(int client_fd);
void set_client_name(int client_fd, const std::string& new_name);
std::string get_current_time();
