#include <sys/socket.h>

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

// 持续读取用户输入并发给服务器；输入 exit 时关闭发送方向。
void send_messages(int sock) {
    while (true) {
        std::string msg;
        std::getline(std::cin, msg);

        // 空输入不发送，避免聊天室里出现空消息。
        if (msg.empty()) {
            continue;
        }

        // 输入 exit 表示用户主动退出。
        //
        // shutdown(SHUT_WR) 只关闭写方向：客户端不再向服务器发送数据，
        // 但接收线程仍可读到服务器在连接关闭前发来的最后消息。
        if (msg == "exit") {
            shutdown(sock, SHUT_WR);
            break;
        }

        send(sock, msg.c_str(), msg.size(), 0);
    }
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
