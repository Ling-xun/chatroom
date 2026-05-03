#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

namespace {

const char* kServerIp = "127.0.0.1";
constexpr int kServerPort = 8080;

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

std::string pack_message(const std::string& msg) {
    uint32_t body_len = msg.size();
    uint32_t net_len = htonl(body_len);

    std::string packet;
    packet.append(reinterpret_cast<const char*>(&net_len), sizeof(net_len));
    packet.append(msg);

    return packet;
}

}  // namespace

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kServerPort);
    addr.sin_addr.s_addr = inet_addr(kServerIp);

    if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return -1;
    }

    std::cout << "connected to server\n";

    // 1. 先注册昵称
    std::string name_packet = pack_message("ProtoTester");
    send_all(sock, name_packet.data(), name_packet.size());

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 2. 粘包测试：两条完整消息一次性发出
    std::string sticky_packet = pack_message("sticky-1") + pack_message("sticky-2");
    send_all(sock, sticky_packet.data(), sticky_packet.size());
    std::cout << "sent sticky packet test\n";

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 3. 半包测试：一条消息拆成两次发送
    std::string half_packet = pack_message("half-packet-message");

    size_t split_pos = 6;
    send_all(sock, half_packet.data(), split_pos);
    std::cout << "sent first half packet\n";

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    send_all(sock, half_packet.data() + split_pos, half_packet.size() - split_pos);
    std::cout << "sent second half packet\n";

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    close(sock);
    return 0;
}