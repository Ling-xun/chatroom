#pragma once

#include <mutex>
#include <string>
#include <vector>
#include <cstddef>

// 保存单个客户端的连接状态与聊天身份信息。
struct ClientInfo {
    // 客户端对应的 socket 文件描述符。
    int sock;

    // 客户端昵称。连接刚建立时为空，注册成功后写入用户输入的昵称。
    std::string name;

    // 是否已经完成昵称注册；未注册时第一条消息会被当作昵称。
    bool registered;

    std::string recv_buffer;
};

// 在线客户端列表，由服务端事件循环和消息处理逻辑共享。
extern std::vector<ClientInfo> clients;

// 保护 clients 的互斥锁；所有读写 clients 的地方都应先加锁。
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

// 获取所有已经完成注册的在线用户昵称。
std::vector<std::string> get_online_users();

bool append_to_client_buffer(int client_fd,const char* msg,size_t len);

bool extract_message_from_client_buffer(int client_fd,std::string& msg);
