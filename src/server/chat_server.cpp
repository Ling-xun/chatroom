#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <ctime>
#include <cstring>
#include <iostream>

#include "server/chat_server.h"
#include "server/client_manager.h"

// 生成统一的时间字符串，供系统提示和聊天消息复用。
std::string get_current_time() {
    time_t now = time(nullptr);
    struct tm* local_time = localtime(&now);
    char time_str[10];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", local_time);
    return std::string(time_str);
}

// 广播时先复制一份客户端快照，避免发送过程中长期占用互斥锁。
void broadcast_msg(int sender_fd, const std::string& msg) {
    std::vector<ClientInfo> snapshot;

    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        snapshot = clients;
    }

    for (const auto& client : snapshot) {
        if (client.sock != sender_fd) {
            // 发送者自己不需要再收到自己发出的广播内容。
            send(client.sock, msg.c_str(), msg.size(), 0);
        }
    }
}
