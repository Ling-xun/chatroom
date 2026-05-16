#include <sys/socket.h>
#include <arpa/inet.h>

#include <cerrno>
#include <iostream>
#include <mutex>
#include <string>

#include "client/chat_client.h"
#include "common/protocol.h"

// chat_client.cpp
//
// 该文件实现客户端的基础网络收发能力：
// 1. send_all 保证一段内存中的数据被完整写入 socket。
// 2. send_message 按“4 字节长度前缀 + 消息体”格式发送消息。
// 3. recv_messages 在独立线程中按同样协议接收服务端广播并打印到终端。

namespace {

// 单次接收服务端消息的最大字节数。
//
// 服务端发给客户端的消息同样使用长度前缀协议，这里每次 recv 只搬运一段原始字节。
constexpr int kBufferSize = 1024;

// 多线程终端输出锁，避免接收线程打印内容时和其他输出交错。
//
// 客户端主线程/发送线程可能正在读用户输入，接收线程也会打印广播消息；加锁可以让
// 单条输出保持完整。
std::mutex cout_mutex;

}  // namespace

// 尽量发送 len 字节数据，直到全部写入 socket 或遇到错误。
//
// send 不保证一次调用就写完所有数据，尤其是在网络缓冲区空间不足时，可能只写入一部分。
// 因此这里循环调用 send，并用 total_sent 记录已经成功发送的字节数。
//
// 返回 true 表示整段数据都已发出；返回 false 表示连接异常或发送失败。
bool send_all(int sock, const char* data, size_t len) {
    size_t total_sent = 0;

    while (total_sent < len) {
        // 从 data + total_sent 继续发送剩余部分，避免重复发送已经成功写入的字节。
        ssize_t n = send(sock, data + total_sent, len - total_sent, 0);

        if (n < 0 && errno == EINTR) {
            continue;
        }

        if (n <= 0) {
            return false;
        }

        total_sent += n;
    }

    return true;
}

// 按聊天室协议发送一条应用层消息。
//
// 客户端发给服务端的消息使用长度前缀协议：
//   [4 字节网络字节序 uint32_t 长度][消息体]
//
// 这样服务端即使遇到 TCP 粘包或半包，也能根据长度头恢复出完整消息边界。
bool send_message(int sock, const std::string& msg) {
    std::string packet = pack_protocol_message(msg);

    // send_all 保证长度头和消息体组成的完整协议帧都写入 socket。
    return send_all(sock, packet.data(), packet.size());
}

// 持续接收服务器广播，并串行化终端输出，避免与输入线程互相打断。
void recv_messages(int sock) {
    std::string recv_buffer;

    while (true) {
        char buffer[kBufferSize];
        int n = recv(sock, buffer, kBufferSize, 0);

        // n == 0 表示服务器关闭连接；n < 0 表示接收出错。两种情况都结束接收线程。
        if (n <= 0) {
            break;
        }

        recv_buffer.append(buffer, n);

        while (true) {
            std::string msg;
            ProtocolExtractResult result = extract_protocol_message(recv_buffer, msg);

            if (result == ProtocolExtractResult::NeedMoreData) {
                break;
            }

            if (result == ProtocolExtractResult::ProtocolError) {
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cerr << "server sent invalid protocol data" << std::endl;
                return;
            }

            // 标准输出是共享资源，加锁后再打印，保证一条消息完整显示。
            //
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << msg;
        }
    }
}
