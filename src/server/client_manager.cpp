#include "server/client_manager.h"
#include <algorithm>

std::vector<ClientInfo> clients;
std::mutex clients_mutex;

std::string get_client_name(int client_fd) {
    // 所有对客户端列表的读写都要加锁，避免并发访问出错。
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (const auto& client : clients) {
        if (client.sock == client_fd) {
            return client.name;
        }
    }
    return "unknown";
}

void set_client_name(int client_fd, const std::string& new_name) {
    // 找到目标连接后更新昵称，未找到时保持原状。
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (auto& client : clients) {
        if (client.sock == client_fd) {
            client.name = new_name;
            break;
        }
    }
}

bool is_client_registered(int client_fd) {
    // 注册状态决定服务器是把第一条消息当昵称，还是当普通聊天内容。
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (const auto& client : clients) {
        if (client.sock == client_fd) {
            return client.registered;
        }
    }
    return false;
}

void set_client_registered(int client_fd, bool value) {
    // 在客户端首次提交昵称后，将其切换为已注册状态。
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (auto& client : clients) {
        if (client.sock == client_fd) {
            client.registered = value;
            break;
        }
    }
}

void remove_client(int client_fd) {
    // 连接断开后，从在线列表中删除对应客户端记录。
    std::lock_guard<std::mutex> lock(clients_mutex);
    auto it = std::find_if(clients.begin(), clients.end(),
        [client_fd](const ClientInfo& client) {
            return client.sock == client_fd;
        });
    if (it != clients.end()) {
        clients.erase(it);
    }
}
