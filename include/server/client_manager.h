#pragma once
#include <vector>
#include <mutex>
#include <string>

struct ClientInfo {
    int sock;
    std::string name;
    bool registered;
};

extern std::vector<ClientInfo> clients;
extern std::mutex clients_mutex;

std::string get_client_name(int client_fd);
void set_client_name(int client_fd, const std::string& new_name);

bool is_client_registered(int client_fd);
void set_client_registered(int client_fd, bool value);

void remove_client(int client_fd);