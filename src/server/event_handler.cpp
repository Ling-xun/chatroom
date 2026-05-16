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
#include "storage/mysql_storage.h"

// event_handler.cpp
//
// 该文件负责处理“某个客户端 fd 可读”这一类 epoll 事件：
// 1. 从 socket 读取原始字节。
// 2. 交给 client_manager 的接收缓冲区做粘包/半包处理。
// 3. 对拆出的完整消息执行注册、命令处理、广播和持久化。
// 4. 在客户端断开或读错误时统一清理连接。

namespace {

// 单次从客户端 socket 读取的最大字节数。
//
// 这不是单条聊天消息的上限，而是一次 recv 最多搬运多少字节到用户态。
// 真正的单条消息上限在 client_manager.cpp 的拆包逻辑中控制。
constexpr int kBufferSize = 1024;

// 统一清理断开的客户端连接。
//
// announce 为 true 时会向其他在线用户广播离开消息；未注册昵称的客户端不会广播，
// 因为聊天室里还没有可展示的用户身份。
void disconnect_client(int epoll_fd, int client_fd, bool announce) {
    if (announce && is_client_registered(client_fd)) {
        // 删除客户端状态前先取昵称，否则 remove_client 后就无法再查到该用户名称。
        std::string name = get_client_name(client_fd);
        std::string leave_msg =
            "[" + get_current_time() + "] [system] " + name + " left the chat\n";

        std::cout << leave_msg;
        broadcast_msg(epoll_fd, client_fd, leave_msg);
    }

    // 从业务在线列表、epoll 监听集合和系统 fd 表中依次移除。
    //
    // 这三个动作对应三层资源：
    // - clients：聊天室业务状态。
    // - epoll：事件循环关注的 fd 集合。
    // - close：操作系统真正持有的 socket fd。
    remove_client(client_fd);
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
    close(client_fd);
}

void disable_client_write_event(int epoll_fd, int client_fd) {
    epoll_event event{};
    event.events = EPOLLIN | EPOLLRDHUP;
    event.data.fd = client_fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client_fd, &event) < 0) {
        perror("epoll_ctl: disable EPOLLOUT");
    }
}

bool handle_client_output(int epoll_fd, int client_fd) {
    ClientFlushResult result = flush_client_send_buffer(client_fd);

    if (result == ClientFlushResult::Complete) {
        disable_client_write_event(epoll_fd, client_fd);
        return true;
    }

    if (result == ClientFlushResult::Pending) {
        return true;
    }

    disconnect_client(epoll_fd, client_fd, true);
    return false;
}

// 处理已经从协议缓冲区拆出来的一条完整应用层消息。
//
// 此时 msg 已经不含 4 字节长度前缀，可以直接按文本命令或聊天内容处理。
void process_client_message(int epoll_fd, int client_fd, std::string msg) {
    // 未注册的连接还没有昵称，因此把它发来的第一条完整消息作为昵称。
    if (!is_client_registered(client_fd)) {
        // 客户端接入后的第一条消息被当作昵称注册。
        if (msg.empty()) {
            msg = "unknown";
        }

        set_client_name(client_fd, msg);
        set_client_registered(client_fd, true);

        // 向当前客户端发送欢迎消息，并向其他人广播加入提示。
        //
        // 欢迎消息只发给自己，加入提示发给其他人；二者都是普通文本输出，
        // 客户端接收线程会直接打印。
        std::string welcome_msg =
            "[" + get_current_time() + "] [system] Welcome, " + msg + "!\n";
        send_msg_to_client(epoll_fd, client_fd, welcome_msg);

        std::string join_msg =
            "[" + get_current_time() + "] [system] " + msg + " joined the chat\n";
        std::cout << join_msg;
        broadcast_msg(epoll_fd, client_fd, join_msg);
    } else {
        // 已注册用户可以先尝试走命令处理；命令处理成功时不会再广播普通消息。
        if (handle_command(epoll_fd, client_fd, msg)) {
            return;
        }

        // 已注册用户的后续消息会附带时间戳和昵称后再广播出去。
        //
        // 服务器端也打印一份，方便运行服务端时观察聊天室日志。
        std::string name = get_client_name(client_fd);
        std::string formatted_msg =
            "[" + get_current_time() + "] [" + name + "]: " + msg + "\n";
        std::cout << formatted_msg;
        broadcast_msg(epoll_fd, client_fd, formatted_msg);

        // 广播成功后再尝试写入数据库。当前代码不因保存失败而影响在线聊天流程。
        g_mysql_storage.save_message(name, msg, "chat");
    }
}

}  // namespace

// 处理一个客户端 socket 的可读事件。
//
// main.cpp 的 epoll_wait 发现某个客户端 fd 可读后，会调用本函数。函数内部会区分：
// - 正常读到数据：追加到客户端缓冲区并尽可能拆出完整消息。
// - 读到 0：对端正常关闭。
// - 读到错误：根据 errno 判断是暂时无数据还是连接异常。
void handle_client_event(int epoll_fd, int client_fd, uint32_t events) {
    if ((events & (EPOLLERR | EPOLLHUP)) != 0) {
        disconnect_client(epoll_fd, client_fd, true);
        return;
    }

    if ((events & EPOLLOUT) != 0 && !handle_client_output(epoll_fd, client_fd)) {
        return;
    }

    if ((events & EPOLLIN) == 0) {
        if ((events & EPOLLRDHUP) != 0) {
            disconnect_client(epoll_fd, client_fd, true);
        }
        return;
    }

    // 读取客户端发来的数据。
    //
    // 多预留 1 个字节，方便后续如果需要时兼容 C 风格字符串处理。
    // 当前实际按 recv 返回的 n 字节追加到客户端缓冲区，不依赖 '\0' 结尾。
    char buffer[kBufferSize + 1];
    int n = recv(client_fd, buffer, kBufferSize, 0);

    if (n == 0) {
        // recv 返回 0 表示对端已经正常关闭连接。
        disconnect_client(epoll_fd, client_fd, true);
        return;
    }

    if (n < 0) {
        // 非阻塞 socket 暂时没有可读数据，不属于错误，等待下一次 epoll 通知即可。
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return;
        }

        // 客户端异常断开连接时，清理本地保存的客户端状态。
        if (errno == ECONNRESET) {
            disconnect_client(epoll_fd, client_fd, true);
            return;
        }

        // 其他 recv 错误也视为当前连接不可继续使用。
        perror("recv");
        disconnect_client(epoll_fd, client_fd, true);
        return;
    }

    // 将本次读取到的原始字节追加到该客户端的协议缓冲区。
    // TCP 是字节流，单次 recv 可能拿到半条、多条或混合消息，因此需要缓冲后按协议拆包。
    if (!append_to_client_buffer(client_fd, buffer, n)) {
        return;
    }

    std::string msg;
    // 从缓冲区中不断提取完整消息，直到剩余内容不足以组成下一条消息。
    while (true) {
        ClientMessageResult result = extract_message_from_client_buffer(client_fd, msg);

        if (result == ClientMessageResult::NeedMoreData) {
            break;
        }

        if (result == ClientMessageResult::ProtocolError) {
            std::cerr << "protocol error from client " << client_fd << std::endl;
            disconnect_client(epoll_fd, client_fd, true);
            break;
        }

        process_client_message(epoll_fd, client_fd, msg);
    }

    if ((events & EPOLLRDHUP) != 0) {
        disconnect_client(epoll_fd, client_fd, true);
    }
}
