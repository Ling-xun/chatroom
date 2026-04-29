#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>

#include <iostream>
#include <string>

#include "server/chat_server.h"
#include "server/client_manager.h"
#include "server/event_handler.h"
#include "server/command_handler.h"

void handle_client_event(int epoll_fd, int client_fd) {
    // 读取客户端发来的数据；这里约定单次最多处理 1024 字节。
    char buffer[1025];
    int n = recv(client_fd, buffer, 1024, 0);

    if (n == 0) {
    if (is_client_registered(client_fd)) {
        std::string name = get_client_name(client_fd);
        std::string leave_msg = "[" + get_current_time() + "] [system] " + name + " left the chat\n";
        std::cout << leave_msg;
        broadcast_msg(client_fd, leave_msg);
    }
    remove_client(client_fd);
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
    close(client_fd);
    return;
}

if (n < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return;
    }

    perror("recv");
    if (is_client_registered(client_fd)) {
        std::string name = get_client_name(client_fd);
        std::string leave_msg = "[" + get_current_time() + "] [system] " + name + " left the chat\n";
        std::cout << leave_msg;
        broadcast_msg(client_fd, leave_msg);
    }
    remove_client(client_fd);
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
    close(client_fd);
    return;
}
    buffer[n] = '\0';
    std::string msg(buffer);

    if (!is_client_registered(client_fd)) {
        // 客户端接入后的第一条消息被当作昵称注册。
        if (msg.empty()) {
            msg = "unknown";
        }

        set_client_name(client_fd, msg);
        set_client_registered(client_fd, true);

        // 向当前客户端发送欢迎消息，并向其他人广播加入提示。
        std::string welcome_msg =
            "[" + get_current_time() + "] [system] Welcome, " + msg + "!\n";
        send(client_fd, welcome_msg.c_str(), welcome_msg.size(), 0);

        std::string join_msg =
            "[" + get_current_time() + "] [system] " + msg + " joined the chat\n";
        std::cout << join_msg;
        broadcast_msg(client_fd, join_msg);
    } else {
        if (handle_command(client_fd, msg)) {
            return;
        }
        // 已注册用户的后续消息会附带时间戳和昵称后再广播出去。
        std::string name = get_client_name(client_fd);
        std::string formatted_msg =
            "[" + get_current_time() + "] [" + name + "]: " + msg + "\n";
        std::cout << formatted_msg;
        broadcast_msg(client_fd, formatted_msg);
   }
}
