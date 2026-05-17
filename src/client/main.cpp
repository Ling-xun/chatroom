#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <iostream>
#include <string>
#include <thread>

#include "client/chat_client.h"
#include "common/config.h"

// main.cpp
//
// 聊天室客户端入口文件。
//
// 主要流程：
// 1. 创建 TCP socket 并连接本机服务端。
// 2. 读取用户昵称，并作为第一条协议消息发给服务端完成注册。
// 3. 启动发送线程读取终端输入，启动接收线程打印服务端广播。
// 4. 用户输入 exit 或标准输入结束时关闭写端并退出。

int main() {
    ClientConfig config = load_client_config();

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
    addr.sin_port = htons(config.server_port);
    addr.sin_addr.s_addr = inet_addr(config.server_ip.c_str());

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
        // 和服务端兜底逻辑保持一致，避免空昵称进入聊天室。
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
                // 标准输入结束时关闭 socket 写方向，通知服务端本客户端不再发送数据。
                shutdown(sock, SHUT_WR);
                break;
            }

            // 空输入不发送，避免聊天室里出现空消息。
            if (msg.empty()) {
                continue;
            }

            // 输入 exit 表示用户主动退出。
            if (msg == "exit") {
                // 只关闭写方向，让接收线程仍有机会读到服务端最后返回的数据或关闭通知。
                shutdown(sock, SHUT_WR);
                break;
            }

            // 普通聊天内容按长度前缀协议发送给服务端。
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
