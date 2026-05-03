#include <sys/socket.h>
#include <arpa/inet.h>

#include <cstdint>
#include <iostream>
#include <mutex>
#include <string>

#include "client/chat_client.h"

namespace {

// 单次接收服务端消息的最大字节数。
constexpr int kBufferSize = 1024;

// 多线程终端输出锁，避免接收线程打印内容时和其他输出交错。
std::mutex cout_mutex;

}  // namespace

bool send_all(int sock, const char* data, size_t len) {
    size_t total_sent = 0;

    while (total_sent < len) {
        ssize_t n = send(sock, data + total_sent, len - total_sent, 0);

        if (n <= 0) {
            return false;
        }

        total_sent += n;
    }

    return true;
}

bool send_message(int sock, const std::string& msg) {
    uint32_t body_len = msg.size();
    uint32_t net_len = htonl(body_len);

    if (!send_all(sock, reinterpret_cast<const char*>(&net_len), sizeof(net_len))) {
        return false;
    }

    if (!send_all(sock, msg.data(), msg.size())) {
        return false;
    }

    return true;
}

// 持续接收服务器广播，并串行化终端输出，避免与输入线程互相打断。
void recv_messages(int sock) {
    while (true) {
        // 多预留 1 个字节用于补 '\0'，让缓冲区可以安全地按字符串打印。
        char buffer[kBufferSize + 1];
        int n = recv(sock, buffer, kBufferSize, 0);

        // n == 0 表示服务器关闭连接；n < 0 表示接收出错。两种情况都结束接收线程。
        if (n <= 0) {
            break;
        }

        buffer[n] = '\0';

        {
            // 标准输出是共享资源，加锁后再打印，保证一条消息完整显示。
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << buffer << std::endl;
        }
    }
}
