#include <arpa/inet.h>
#include <sys/socket.h>

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

// 向除发送者之外的所有在线客户端广播消息。
//
// sender_fd 用来标识消息来源：
// - 普通聊天时，发送者不需要重复收到自己刚输入的内容。
// - 系统加入/离开提示也会跳过对应客户端，避免语义重复。
//
// msg 应该已经由调用方拼好换行、时间戳、昵称等展示内容。
void broadcast_msg(int sender_fd, const std::string& msg) {
    std::vector<ClientInfo> snapshot;

    {
        // 广播时先复制一份客户端快照，避免发送过程中长期占用互斥锁。
        //
        // send 可能因为网络缓冲区、对端状态等原因耗时或失败。如果一直持锁发送，
        // 其他线程/流程就无法查询或更新在线列表。复制后释放锁，可以把锁的持有时间
        // 控制在非常短的范围内。
        std::lock_guard<std::mutex> lock(clients_mutex);
        snapshot = clients;
    }

    for (const auto& client : snapshot) {
        if (client.sock != sender_fd) {
            // 这里直接发送原始文本，不再附加长度头，因为服务端发给客户端的展示消息
            // 当前按普通文本流处理，客户端 recv 后直接打印。
            //
            // send 返回值当前被忽略：如果连接已经异常，后续 recv/epoll 流程会统一发现
            // 并清理该客户端。对于简单聊天室，这样可以让广播逻辑保持轻量。
            send(client.sock, msg.c_str(), msg.size(), 0);
        }
    }
}
