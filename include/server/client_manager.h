#pragma once

#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

// client_manager.h
//
// 声明服务端客户端状态管理接口：
// - 维护在线客户端列表。
// - 查询和修改客户端昵称、注册状态。
// - 维护每个客户端的 TCP 接收缓冲区，支持长度前缀协议拆包。

// 保存单个客户端的连接状态与聊天身份信息。
struct ClientInfo {
    // 客户端对应的 socket 文件描述符。
    int sock;

    // 客户端昵称。连接刚建立时为空，注册成功后写入用户输入的昵称。
    std::string name;

    // 是否已经完成昵称注册；未注册时第一条消息会被当作昵称。
    bool registered;

    // 该客户端尚未拆包完成的原始接收数据。
    std::string recv_buffer;

    // 该客户端尚未写入 socket 的协议帧数据。
    std::string send_buffer;
};

enum class ClientMessageResult {
    // 成功从接收缓冲区中提取出一条完整应用层消息。
    Message,

    // 当前数据还不够组成完整协议帧，等待后续 recv 继续追加。
    NeedMoreData,

    // 消息长度或协议格式异常，调用方应关闭该客户端连接。
    ProtocolError
};

enum class ClientFlushResult {
    // 发送缓冲区已经全部写入 socket，可以关闭 EPOLLOUT 监听。
    Complete,

    // socket 暂时不可继续写，保留 EPOLLOUT，等待下一次可写通知。
    Pending,

    // 对端已经断开或发送管道失效，调用方应清理连接。
    Disconnected,

    // 其他发送错误，调用方应按异常连接处理。
    Error
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

// 将本次 recv 得到的原始字节追加到指定客户端的接收缓冲区。
bool append_to_client_buffer(int client_fd, const char* msg, size_t len);

// 从指定客户端接收缓冲区中尝试提取一条完整消息。
ClientMessageResult extract_message_from_client_buffer(int client_fd, std::string& msg);

// 将一条展示消息打包后追加到指定客户端的发送缓冲区。
bool queue_client_message(int client_fd, const std::string& msg);

// 尽量把指定客户端发送缓冲区中的数据写入 socket。
ClientFlushResult flush_client_send_buffer(int client_fd);
