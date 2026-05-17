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
// 该文件负责处理某个客户端 fd 上的 epoll 事件：
// 1. 连接异常或关闭时统一清理资源。
// 2. socket 可写时刷新发送缓冲区。
// 3. socket 可读时读取原始字节，并交给 client_manager 的接收缓冲区做粘包/半包处理。
// 4. 对拆出的完整消息执行注册、命令处理、广播和持久化。

namespace {

// 单次从客户端 socket 读取的最大字节数。
//
// 这不是单条聊天消息的上限，而是一次 recv 最多搬运多少字节到用户态。
// 真正的单条消息上限在 client_manager.cpp 的拆包逻辑中控制。
constexpr int kBufferSize = 1024;

enum class ClientReadStatus {
    // 本次 recv 成功读到字节，bytes_read 表示实际读取长度。
    Data,

    // 非阻塞 socket 暂时无数据可读，不需要关闭连接。
    WouldBlock,

    // 对端正常关闭、重置连接，或出现不可恢复的读取错误。
    Disconnected
};

struct ClientReadResult {
    ClientReadStatus status;

    // 仅当 status 为 Data 时有效。
    int bytes_read;
};

// 判断 epoll 返回的事件集合中是否包含指定事件位。
bool has_event(uint32_t events, uint32_t mask) {
    return (events & mask) != 0;
}

// 统一生成系统提示消息，调用方只传入提示正文。
std::string make_system_message(const std::string& content) {
    return "[" + get_current_time() + "] [system] " + content + "\n";
}

// 统一清理断开的客户端连接。
//
// announce 为 true 时会向其他在线用户广播离开消息；未注册昵称的客户端不会广播，
// 因为聊天室里还没有可展示的用户身份。
void disconnect_client(int epoll_fd, int client_fd, bool announce) {
    if (announce && is_client_registered(client_fd)) {
        // 删除客户端状态前先取昵称，否则 remove_client 后就无法再查到该用户名称。
        std::string name = get_client_name(client_fd);
        std::string leave_msg = make_system_message(name + " left the chat");

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

// 发送缓冲区已经清空后，关闭 EPOLLOUT，避免 socket 持续触发“可写”事件。
void disable_client_write_event(int epoll_fd, int client_fd) {
    epoll_event event{};
    event.events = EPOLLIN | EPOLLRDHUP;
    event.data.fd = client_fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client_fd, &event) < 0) {
        perror("epoll_ctl: disable EPOLLOUT");
    }
}

// 处理客户端 socket 的可写事件。
//
// 返回 false 表示连接已经不可继续使用，并且函数内部已经完成清理。
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

// 完成新客户端注册。
//
// 约定客户端发来的第一条完整协议消息就是昵称；注册后才会进入普通聊天/命令流程。
void register_client(int epoll_fd, int client_fd, std::string name) {
    if (name.empty()) {
        name = "unknown";
    }

    set_client_name(client_fd, name);
    set_client_registered(client_fd, true);

    // 欢迎消息只发给自己，加入提示发给其他人；二者都是普通文本输出，
    // 客户端接收线程会直接打印。
    std::string welcome_msg = make_system_message("Welcome, " + name + "!");
    send_msg_to_client(epoll_fd, client_fd, welcome_msg);

    std::string join_msg = make_system_message(name + " joined the chat");
    std::cout << join_msg;
    broadcast_msg(epoll_fd, client_fd, join_msg);
}

// 处理已完成注册用户的普通输入。
//
// 命令消息会被 command_handler 消费；非命令消息会格式化、广播并尝试落库。
void handle_registered_client_message(int epoll_fd, int client_fd, const std::string& msg) {
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

// 处理已经从协议缓冲区拆出来的一条完整应用层消息。
//
// 此时 msg 已经不含 4 字节长度前缀，可以直接按文本命令或聊天内容处理。
void process_client_message(int epoll_fd, int client_fd, std::string msg) {
    // 未注册的连接还没有昵称，因此把它发来的第一条完整消息作为昵称。
    if (!is_client_registered(client_fd)) {
        register_client(epoll_fd, client_fd, msg);
        return;
    }

    handle_registered_client_message(epoll_fd, client_fd, msg);
}

// 优先处理 EPOLLERR/EPOLLHUP。
//
// 这类事件表示 fd 状态已经异常，继续读写没有意义，直接走统一断开流程。
bool handle_connection_error_event(int epoll_fd, int client_fd, uint32_t events) {
    if (!has_event(events, EPOLLERR | EPOLLHUP)) {
        return false;
    }

    disconnect_client(epoll_fd, client_fd, true);
    return true;
}

// 如果本轮事件包含 EPOLLOUT，就尝试刷新发送缓冲区。
//
// 返回 true 表示事件处理可以继续；返回 false 表示连接已经被清理。
bool handle_write_event(int epoll_fd, int client_fd, uint32_t events) {
    if (!has_event(events, EPOLLOUT)) {
        return true;
    }

    return handle_client_output(epoll_fd, client_fd);
}

// 从客户端 socket 读取一段原始字节，并把 recv 的几类返回值归一成 ClientReadResult。
ClientReadResult read_client_data(int client_fd, char* buffer) {
    int n = recv(client_fd, buffer, kBufferSize, 0);

    if (n > 0) {
        return {ClientReadStatus::Data, n};
    }

    if (n == 0) {
        return {ClientReadStatus::Disconnected, 0};
    }

    // 非阻塞 socket 暂时没有可读数据，不属于错误，等待下一次 epoll 通知即可。
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return {ClientReadStatus::WouldBlock, 0};
    }

    // 客户端异常断开连接时，清理本地保存的客户端状态。
    if (errno != ECONNRESET) {
        perror("recv");
    }

    return {ClientReadStatus::Disconnected, 0};
}

// 将 socket 字节追加到该客户端的协议缓冲区。
bool append_client_data(int client_fd, const char* buffer, int bytes_read) {
    // TCP 是字节流，单次 recv 可能拿到半条、多条或混合消息，因此需要缓冲后按协议拆包。
    return append_to_client_buffer(client_fd, buffer, static_cast<size_t>(bytes_read));
}

// 持续消费客户端协议缓冲区中的完整消息。
//
// 一次 recv 可能带来多条协议消息，所以这里循环拆包，直到缓冲区不足以组成下一条消息。
bool drain_client_messages(int epoll_fd, int client_fd) {
    std::string msg;

    // 从缓冲区中不断提取完整消息，直到剩余内容不足以组成下一条消息。
    while (true) {
        ClientMessageResult result = extract_message_from_client_buffer(client_fd, msg);

        if (result == ClientMessageResult::NeedMoreData) {
            return true;
        }

        if (result == ClientMessageResult::ProtocolError) {
            std::cerr << "protocol error from client " << client_fd << std::endl;
            disconnect_client(epoll_fd, client_fd, true);
            return false;
        }

        process_client_message(epoll_fd, client_fd, msg);
    }
}

// 处理客户端 socket 的可读事件。
//
// 函数内部完成“recv -> 追加缓冲区 -> 拆完整消息 -> 分发业务处理”的完整链路。
bool handle_read_event(int epoll_fd, int client_fd) {
    // 多预留 1 个字节，方便后续如果需要时兼容 C 风格字符串处理。
    // 当前实际按 recv 返回的 n 字节追加到客户端缓冲区，不依赖 '\0' 结尾。
    char buffer[kBufferSize + 1];
    ClientReadResult read_result = read_client_data(client_fd, buffer);

    if (read_result.status == ClientReadStatus::WouldBlock) {
        return false;
    }

    if (read_result.status == ClientReadStatus::Disconnected) {
        disconnect_client(epoll_fd, client_fd, true);
        return false;
    }

    if (!append_client_data(client_fd, buffer, read_result.bytes_read)) {
        return false;
    }

    return drain_client_messages(epoll_fd, client_fd);
}

// 只有本轮事件真正包含 EPOLLIN 时才读 socket。
bool handle_read_event_if_needed(int epoll_fd, int client_fd, uint32_t events) {
    if (!has_event(events, EPOLLIN)) {
        return true;
    }

    return handle_read_event(epoll_fd, client_fd);
}

// 处理对端关闭写方向的事件。
//
// EPOLLRDHUP 通常表示对端不再发送数据；读事件处理完后再清理，可以尽量消费已到达的数据。
void handle_peer_shutdown_event(int epoll_fd, int client_fd, uint32_t events) {
    if (has_event(events, EPOLLRDHUP)) {
        disconnect_client(epoll_fd, client_fd, true);
    }
}

}  // namespace

// 处理一个客户端 socket 的 epoll 事件。
//
// main.cpp 的 epoll_wait 发现某个客户端 fd 就绪后，会调用本函数。函数内部会区分：
// - 连接异常或对端关闭：统一清理客户端资源。
// - 可写：尽量把发送缓冲区中的数据写入 socket。
// - 可读：读取字节、追加协议缓冲区并尽可能拆出完整消息。
void handle_client_event(int epoll_fd, int client_fd, uint32_t events) {
    if (handle_connection_error_event(epoll_fd, client_fd, events)) {
        return;
    }

    if (!handle_write_event(epoll_fd, client_fd, events)) {
        return;
    }

    if (!handle_read_event_if_needed(epoll_fd, client_fd, events)) {
        return;
    }

    handle_peer_shutdown_event(epoll_fd, client_fd, events);
}
