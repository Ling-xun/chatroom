#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <iostream>
#include <string>
#include <thread>

#include "client/chat_client.h"

namespace {

// 客户端默认连接本机服务器；如果服务端部署到其他机器，只需要改这里。
const char* kServerIp = "127.0.0.1";

// 需要与服务端监听端口保持一致。
constexpr int kServerPort = 8080;

}  // namespace

int main() {
    // 1. 创建客户端 socket，并准备连接聊天服务器。
    //
    // AF_INET 表示 IPv4，SOCK_STREAM 表示 TCP。创建成功后 sock 就是后续通信使用的 fd。
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    // 2. 组装服务器地址。
    //
    // htons 用于把端口转成网络字节序；inet_addr 用于把点分十进制 IP 转成二进制地址。
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kServerPort);
    addr.sin_addr.s_addr = inet_addr(kServerIp);

    // 3. 主动连接服务端。连接失败时关闭 socket 后退出。
    if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return -1;
    }

    // 4. 首次输入的昵称会被服务器用作用户注册信息。
    std::string name;
    std::cout << "请输入您的姓名：";
    getline(std::cin, name);
    std::cout << "\n";

    if (name.empty()) {
        name = "unknown";
    }

    if (!send_message(sock, name)) {
        perror("send name");
        close(sock);
        return -1;
    }

    // 5. 分别开启发送线程和接收线程，实现终端输入与消息显示并行进行。
    //
    // sender 负责读取键盘输入并发送给服务端；receiver 负责接收服务端广播并打印。
    std::thread sender([sock]() {
        while (true) {
            std::string msg;
            if (!getline(std::cin, msg)) {
                shutdown(sock, SHUT_WR);
                break;
            }

            // 空输入不发送，避免聊天室里出现空消息。
            if (msg.empty()) {
                continue;
            }

            // 输入 exit 表示用户主动退出。
            if (msg == "exit") {
                shutdown(sock, SHUT_WR);
                break;
            }

           if (!send_message(sock, msg)) {
                std::cerr << "send message failed" << std::endl;
                break;
            }
        }
    });
    std::thread receiver(recv_messages, sock);

    sender.join();
    receiver.join();

    // 6. 两个线程结束后再关闭 socket，完成客户端退出。
    close(sock);
    return 0;
}
