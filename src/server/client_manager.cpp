#include "server/client_manager.h"

#include <algorithm>
#include <mutex>
#include <string>
#include <vector>
#include <arpa/inet.h>
#include <cstring>

// 保存所有当前连接到服务器的客户端。
// 这个示例项目使用一个全局列表，让事件处理、命令处理和广播逻辑都能共享在线状态。
std::vector<ClientInfo> clients;

// clients 的配套互斥锁。虽然主事件循环是单线程，但广播、命令处理等函数共享同一份状态，
// 保持统一加锁可以让后续扩展到多线程时更容易维护。
std::mutex clients_mutex;

std::string get_client_name(int client_fd) {
    // 所有对客户端列表的读写都要加锁，避免并发访问出错。
    std::lock_guard<std::mutex> lock(clients_mutex);

    for (const auto& client : clients) {
        if (client.sock == client_fd) {
            return client.name;
        }
    }

    // 如果连接已经断开或未记录，返回一个兜底名称，避免上层拼消息时拿到空值。
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

    auto it = std::find_if(clients.begin(), clients.end(), [client_fd](const ClientInfo& client) {
        return client.sock == client_fd;
    });

    if (it != clients.end()) {
        clients.erase(it);
    }
}

std::vector<std::string> get_online_users() {
    std::lock_guard<std::mutex> lock(clients_mutex);

    std::vector<std::string> online_users;
    for (const auto& client : clients) {
        // 只展示已经完成昵称注册的用户；刚连上但还没发昵称的连接不算正式在线用户。
        if (client.registered) {
            online_users.push_back(client.name);
        }
    }
    return online_users;
}

bool append_to_client_buffer(int client_fd, const char* data, size_t len) {
    std::lock_guard<std::mutex> lock(clients_mutex);

    for (auto& client : clients) {
        if (client.sock == client_fd) {
            client.recv_buffer.append(data, len);
            return true;
        }
    }

    return false;
}

bool extract_message_from_client_buffer(int client_fd, std::string& msg) {
    std::lock_guard<std::mutex> lock(clients_mutex);

    constexpr uint32_t kMaxMessageSize = 4096;

    for (auto& client : clients) {
        if (client.sock == client_fd) {
            if (client.recv_buffer.size() < sizeof(uint32_t)) {
                return false;
            }

            uint32_t net_len = 0;
            std::memcpy(&net_len, client.recv_buffer.data(), sizeof(net_len));

            uint32_t body_len = ntohl(net_len);

            if (body_len > kMaxMessageSize) {
                return false;
            }

            if (client.recv_buffer.size() < sizeof(uint32_t) + body_len) {
                return false;
            }

            msg = client.recv_buffer.substr(sizeof(uint32_t), body_len);
            client.recv_buffer.erase(0, sizeof(uint32_t) + body_len);

            return true;
        }
    }

    return false;
}