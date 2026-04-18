#include "server/client_manager.h"
#include <algorithm>

std::vector<ClientInfo> clients;
std::mutex clients_mutex;

std::string get_client_name(int client_fd) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (const auto& client : clients) {
        if (client.sock == client_fd) {
            return client.name;
        }
    }
    return "unknown";
}

void set_client_name(int client_fd, const std::string& new_name) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (auto& client : clients) {
        if (client.sock == client_fd) {
            client.name = new_name;
            break;
        }
    }
}

bool is_client_registered(int client_fd) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (const auto& client : clients) {
        if (client.sock == client_fd) {
            return client.registered;
        }
    }
    return false;
}

void set_client_registered(int client_fd, bool value) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (auto& client : clients) {
        if (client.sock == client_fd) {
            client.registered = value;
            break;
        }
    }
}

void remove_client(int client_fd) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    auto it = std::find_if(clients.begin(), clients.end(),
        [client_fd](const ClientInfo& client) {
            return client.sock == client_fd;
        });
    if (it != clients.end()) {
        clients.erase(it);
    }
}