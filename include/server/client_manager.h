#pragma once
#include <vector>
#include <mutex>
#include <string>

// 保存单个客户端的连接状态与聊天身份信息。
struct ClientInfo {
    int sock;
    std::string name;
    bool registered;
};

// 在线客户端列表，由服务端事件循环和消息处理逻辑共享。
extern std::vector<ClientInfo> clients;
extern std::mutex clients_mutex;

// 查询指定客户端当前记录的昵称。
std::string get_client_name(int client_fd);

// 更新指定客户端的昵称。
void set_client_name(int client_fd, const std::string& new_name);

// 判断客户端是否已经完成昵称注册。
bool is_client_registered(int client_fd);

// 设置客户端的注册状态。
void set_client_registered(int client_fd, bool value);

// 从在线客户端列表中移除指定连接。
void remove_client(int client_fd);

std::vector<std::string> get_online_users();