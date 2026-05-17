#include <sys/epoll.h>

#include <ctime>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#include "server/chat_server.h"
#include "server/client_manager.h"

// chat_server.cpp
//
// 该文件放置服务端中比较通用的聊天室辅助能力：
// 1. 生成统一格式的本地时间字符串。
// 2. 将一条已经格式化好的消息广播给除发送者外的其他在线客户端。
//
// 这里不关心消息来自普通聊天、系统提示还是命令结果；调用方负责拼好文本，
// 本文件只负责“什么时候”和“发给谁”。

// 生成统一的时间字符串，供系统提示和聊天消息复用。
//
// 返回格式固定为 HH:MM:SS，例如 14:05:09。这样 event_handler、command_handler
// 等模块拼接消息时可以得到一致的时间前缀。
std::string get_current_time() {
    // time(nullptr) 取得当前 Unix 时间戳，localtime 将其转换为本地时区的日历时间。
    time_t now = time(nullptr);
    tm* local_time = localtime(&now);

    // "HH:MM:SS" 共 8 个可见字符，加上字符串结尾 '\0'，10 字节空间足够。
    char time_str[10];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", local_time);

    return std::string(time_str);
}

// 将消息加入客户端发送队列，并让 epoll 后续通知该 fd 的可写事件。
//
// 这里不直接阻塞写 socket，避免一个慢客户端拖住整个服务端事件循环。
bool send_msg_to_client(int epoll_fd, int client_fd, const std::string& msg) {
    if (!queue_client_message(client_fd, msg)) {
        return false;
    }

    epoll_event event{};
    // 保留 EPOLLIN/EPOLLRDHUP，同时额外开启 EPOLLOUT，确保仍能继续收消息和感知断开。
    event.events = EPOLLIN | EPOLLRDHUP | EPOLLOUT;
    event.data.fd = client_fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client_fd, &event) < 0) {
        perror("epoll_ctl: enable EPOLLOUT");
        return false;
    }

    return true;
}

// 向除发送者之外的所有在线客户端广播消息。
//
// sender_fd 用来标识消息来源：
// - 普通聊天时，发送者不需要重复收到自己刚输入的内容。
// - 系统加入/离开提示也会跳过对应客户端，避免语义重复。
//
// msg 应该已经由调用方拼好换行、时间戳、昵称等展示内容。
void broadcast_msg(int epoll_fd, int sender_fd, const std::string& msg) {
    std::vector<int> recipient_fds;

    {
        // 广播时先复制一份 fd 快照，避免发送过程中长期占用互斥锁。
        //
        // 真正的 socket 写入由 EPOLLOUT 触发，当前这里只负责把消息加入发送缓冲区。
        std::lock_guard<std::mutex> lock(clients_mutex);
        for (const auto& client : clients) {
            if (client.sock != sender_fd) {
                recipient_fds.push_back(client.sock);
            }
        }
    }

    for (int client_fd : recipient_fds) {
        send_msg_to_client(epoll_fd, client_fd, msg);
    }
}
