#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <mutex>

#include "server/chat_server.h"
#include "server/client_manager.h"
#include "server/event_handler.h"

namespace {

// 服务端监听端口，客户端需要连接到这个端口才能进入聊天室。
constexpr int kServerPort = 8080;

// listen 的等待队列长度：系统会暂存已经完成 TCP 握手、等待 accept 的连接。
constexpr int kListenBacklog = 5;

// 单次 epoll_wait 最多取出的就绪事件数量。
constexpr int kMaxEvents = 1024;

}  // namespace

// 将 socket 设置为非阻塞模式。
//
// epoll 通常会和非阻塞 socket 搭配使用：当某次读写暂时无法完成时，
// 系统调用会立即返回，而不是把整个服务器事件循环卡住。
int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        perror("fcntl: F_GETFL");
        return -1;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("fcntl: F_SETFL");
        return -1;
    }

    return 0;
}

int main() {
    // 1. 创建 TCP 监听 socket，作为聊天室服务器接收新连接的入口。
    //
    // AF_INET 表示使用 IPv4；SOCK_STREAM 表示使用面向连接的 TCP。
    // 第三个参数传 0 时，系统会根据前两个参数自动选择默认协议。
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return -1;
    }

    // 允许服务重启后快速复用同一个端口，避免端口处于 TIME_WAIT 等状态时绑定失败。
    int reuse_addr = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr)) < 0) {
        perror("setsockopt: SO_REUSEADDR");
        close(server_fd);
        return -1;
    }

    // 2. 配置服务器监听地址：监听本机所有网卡的固定端口。
    //
    // sockaddr_in 是 IPv4 专用地址结构；htons 会把主机字节序转换为网络字节序；
    // INADDR_ANY 表示不限定某一块网卡，127.0.0.1 和局域网 IP 都可以接入。
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kServerPort);
    addr.sin_addr.s_addr = INADDR_ANY;

    // 3. 将 socket 绑定到指定 IP 和端口。
    // 绑定成功后，这个 fd 才真正占用上面配置的服务端地址。
    if (bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return -1;
    }

    // 4. 开始监听客户端连接请求，等待后续 accept。
    if (listen(server_fd, kListenBacklog) < 0) {
        perror("listen");
        close(server_fd);
        return -1;
    }

    // 5. 设置监听 socket 为非阻塞模式，确保 accept 不会阻塞主事件循环。
    if (set_nonblocking(server_fd) < 0) {
        close(server_fd);
        return -1;
    }

    // 6. 打印服务器启动信息，提示客户端连接。
    std::cout << "server start at port " << kServerPort << std::endl;

    // 7. 创建 epoll 实例，用于统一管理监听 socket 和所有客户端 socket 的事件。
    //
    // 服务器不再为每个连接创建单独阻塞等待点，而是让 epoll_wait 一次性返回
    // “哪些 fd 已经准备好”，再根据 fd 类型分别处理新连接或客户端消息。
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        perror("epoll_create1");
        close(server_fd);
        return -1;
    }

    // 8. 把监听 socket 加入 epoll。
    //
    // EPOLLIN 对监听 socket 的含义是：有新客户端连接已经到达，可以 accept。
    epoll_event server_event{};
    server_event.events = EPOLLIN;
    server_event.data.fd = server_fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &server_event) < 0) {
        perror("epoll_ctl: server_fd");
        close(epoll_fd);
        close(server_fd);
        return -1;
    }

    // 用于接收本轮 epoll_wait 返回的就绪事件列表。
    epoll_event events[kMaxEvents];

    // 主事件循环：持续等待新连接或已有客户端发来消息。
    while (true) {
        // 第四个参数 -1 表示一直等待，直到至少有一个事件就绪。
        int nfds = epoll_wait(epoll_fd, events, kMaxEvents, -1);
        if (nfds < 0) {
            // 被信号中断时不代表服务器出错，继续下一轮等待即可。
            if (errno == EINTR) {
                continue;
            }

            perror("epoll_wait");
            continue;
        }

        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == server_fd) {
                // 监听 socket 可读，说明有新的客户端正在建立连接。
                int client_fd = accept(server_fd, nullptr, nullptr);
                if (client_fd < 0) {
                    // 非阻塞 accept 在没有可取连接时会返回 EAGAIN/EWOULDBLOCK。
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        continue;
                    }

                    perror("accept");
                    continue;
                }

                // 客户端 fd 同样设置为非阻塞，避免单个客户端读写拖住整个聊天室。
                if (set_nonblocking(client_fd) < 0) {
                    close(client_fd);
                    continue;
                }

                {
                    // 将新客户端加入全局客户端列表。
                    //
                    // name 暂时为空，registered 为 false。客户端发送的第一条消息会在
                    // handle_client_event 中被当作昵称，之后才切换为正式聊天消息。
                    std::lock_guard<std::mutex> lock(clients_mutex);
                    clients.push_back({client_fd, "", false, ""});
                }

                // 把新客户端注册到 epoll，后续收到消息时会触发 EPOLLIN。
                epoll_event client_event{};
                client_event.events = EPOLLIN;
                client_event.data.fd = client_fd;

                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_event) < 0) {
                    perror("epoll_ctl: client_fd");
                    close(client_fd);
                    remove_client(client_fd);
                    continue;
                }
            } else {
                // 普通客户端 socket 触发事件，交给专门的事件处理函数处理。
                int client_fd = events[i].data.fd;
                handle_client_event(epoll_fd, client_fd);
            }
        }
    }

    close(server_fd);
    return 0;
}
