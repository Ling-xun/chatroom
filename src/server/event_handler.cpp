#include <cerrno>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <string>

#include "server/command_handler.h"
#include "server/chat_server.h"
#include "server/client_manager.h"
#include "server/event_handler.h"

namespace {

// 单次从客户端 socket 读取的最大字节数。
constexpr int kBufferSize = 1024;

// 统一清理断开的客户端连接。
//
// announce 为 true 时会向其他在线用户广播离开消息；未注册昵称的客户端不会广播，
// 因为聊天室里还没有可展示的用户身份。
void disconnect_client(int epoll_fd, int client_fd, bool announce) {
    if (announce && is_client_registered(client_fd)) {
        std::string name = get_client_name(client_fd);
        std::string leave_msg =
            "[" + get_current_time() + "] [system] " + name + " left the chat\n";

        std::cout << leave_msg;
        broadcast_msg(client_fd, leave_msg);
    }

    // 从业务在线列表、epoll 监听集合和系统 fd 表中依次移除。
    remove_client(client_fd);
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
    close(client_fd);
}

void process_client_message(int client_fd, std::string msg) {
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
        // 已注册用户可以先尝试走命令处理；命令处理成功时不会再广播普通消息。
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

}  // namespace

void handle_client_event(int epoll_fd, int client_fd) {
    // 读取客户端发来的数据。
    //
    // 多预留 1 个字节用于写入 '\0'，方便后面把 C 风格缓冲区转成 std::string。
    char buffer[kBufferSize + 1];
    int n = recv(client_fd, buffer, kBufferSize, 0);

    if (n == 0) {
        // recv 返回 0 表示对端已经正常关闭连接。
        disconnect_client(epoll_fd, client_fd, true);
        return;
    }

    if (n < 0) {
        // 非阻塞 socket 在暂时没有数据可读时会返回 EAGAIN/EWOULDBLOCK。
        // 这不算断线，直接等待下一次 epoll 通知即可。
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return;
        }

        perror("recv");
        disconnect_client(epoll_fd, client_fd, true);
        return;
    }

    if (!append_to_client_buffer(client_fd, buffer, n)) {
        return;
    }

    std::string msg;
    while (extract_message_from_client_buffer(client_fd, msg)) {
        process_client_message(client_fd, msg);
    }
}
