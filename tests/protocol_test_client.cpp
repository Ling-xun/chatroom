#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

#include "common/config.h"

// protocol_test_client.cpp
//
// 这是一个用于验证服务端长度前缀协议处理能力的小测试客户端。
//
// 它不会读取用户输入，而是按固定顺序发送几组数据：
// 1. 正常发送昵称完成注册。
// 2. 一次 send 发送两条完整消息，模拟 TCP 粘包。
// 3. 把一条完整消息拆成两次 send，模拟 TCP 半包。
//
// 运行服务端后启动该程序，可以在服务端日志或其他客户端中观察消息是否被正确拆包。

namespace {

// 循环发送，保证测试数据完整写入 socket。
bool send_all(int sock, const char* data, size_t len) {
    size_t total_sent = 0;

    while (total_sent < len) {
        // send 可能只写入剩余数据的一部分，因此成功后继续发送未完成的尾部。
        ssize_t n = send(sock, data + total_sent, len - total_sent, 0);
        if (n <= 0) {
            return false;
        }

        total_sent += n;
    }

    return true;
}

// 将一条文本消息打包为服务端协议所需格式：
//   [4 字节网络字节序长度][消息体]
std::string pack_message(const std::string& msg) {
    uint32_t body_len = msg.size();
    uint32_t net_len = htonl(body_len);

    std::string packet;
    // 长度头是二进制数据，直接 append 4 个字节；随后追加文本消息体。
    packet.append(reinterpret_cast<const char*>(&net_len), sizeof(net_len));
    packet.append(msg);

    return packet;
}

}  // namespace

int main() {
    ClientConfig config = load_client_config();

    // 创建 TCP socket，作为测试客户端与服务端通信的连接。
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    // 组装服务端地址。
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config.server_port);
    addr.sin_addr.s_addr = inet_addr(config.server_ip.c_str());

    // 连接服务端；失败时释放 socket 后退出。
    if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return -1;
    }

    std::cout << "connected to server\n";

    // 1. 先注册昵称。
    //
    // 服务端把每个新连接的第一条完整消息当作昵称，因此测试客户端先发送 ProtoTester。
    std::string name_packet = pack_message("ProtoTester");
    send_all(sock, name_packet.data(), name_packet.size());

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 2. 粘包测试：两条完整消息一次性发出。
    //
    // 如果服务端拆包正确，应该能连续处理 sticky-1 和 sticky-2 两条聊天消息。
    std::string sticky_packet = pack_message("sticky-1") + pack_message("sticky-2");
    send_all(sock, sticky_packet.data(), sticky_packet.size());
    std::cout << "sent sticky packet test\n";

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 3. 半包测试：一条消息拆成两次发送。
    //
    // 服务端第一次只收到部分协议包时不应该处理；第二次补齐后才应输出完整消息。
    std::string half_packet = pack_message("half-packet-message");

    // split_pos 故意落在协议包中间，模拟网络层任意切分 TCP 字节流的情况。
    size_t split_pos = 6;
    send_all(sock, half_packet.data(), split_pos);
    std::cout << "sent first half packet\n";

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    send_all(sock, half_packet.data() + split_pos, half_packet.size() - split_pos);
    std::cout << "sent second half packet\n";

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // 关闭写方向并稍等片刻，让服务端有机会处理断开流程。
    shutdown(sock, SHUT_WR);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    close(sock);
    return 0;
}
