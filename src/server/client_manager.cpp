#include "server/client_manager.h"

#include <algorithm>
#include <mutex>
#include <string>
#include <vector>
#include <arpa/inet.h>
#include <cstring>

// client_manager.cpp
//
// 该文件负责维护服务端眼中的“在线客户端状态”：
// 1. 保存每个已连接客户端的 socket、昵称、注册状态和接收缓冲区。
// 2. 提供按 socket 查询/修改客户端信息的辅助函数。
// 3. 为 TCP 字节流协议维护 per-client 缓冲区，并从中拆出完整消息。
//
// 注意：这里不负责 accept 新连接、epoll 事件处理、消息广播或数据库落库；
// 这些逻辑分别在 chat_server/event_handler/command_handler 等模块中完成。

// 保存所有当前连接到服务器的客户端。
//
// 这里使用全局 vector 的原因是：事件处理、命令处理、广播逻辑都需要共享同一份
// 在线状态。每个元素对应一个客户端连接，元素生命周期通常是：
// 新连接接入时 push_back -> 聊天过程中查询/更新 -> 断开连接时 remove_client 删除。
std::vector<ClientInfo> clients;

// clients 的配套互斥锁。
//
// 当前服务端主要由 epoll 事件循环驱动，但多个函数都会读写 clients。统一在这里加锁
// 可以保证访问约定清晰，也方便以后把广播、命令处理或存储等流程拆到其他线程时复用。
// 只要访问 clients 或 ClientInfo 内部字段，都应持有这把锁。
std::mutex clients_mutex;

// 根据 socket 文件描述符查询客户端昵称。
//
// client_fd 是客户端连接对应的 socket fd，也是 clients 中查找客户端的主键。
// 返回值：
// - 找到客户端时返回其当前昵称。
// - 找不到时返回 "unknown"，避免调用方拼接系统消息或聊天消息时拿到空字符串。
std::string get_client_name(int client_fd) {
    // 使用 lock_guard 做 RAII 加锁：函数返回或异常退出时会自动释放互斥锁。
    std::lock_guard<std::mutex> lock(clients_mutex);

    // 线性扫描在线列表，寻找 socket fd 匹配的客户端。
    // 当前聊天室规模较小，vector + 顺序查找足够简单直接；如果在线人数很多，
    // 可以考虑改为 unordered_map<int, ClientInfo> 来降低查找成本。
    for (const auto& client : clients) {
        if (client.sock == client_fd) {
            return client.name;
        }
    }

    // 可能出现的场景：客户端已经断开并被移除，或调用方传入了未知 fd。
    return "unknown";
}

// 更新指定客户端的昵称。
//
// 该函数只修改内存中的在线状态，不会主动通知其他客户端；是否发送提示或广播由
// command_handler/event_handler 等上层逻辑决定。
void set_client_name(int client_fd, const std::string& new_name) {
    std::lock_guard<std::mutex> lock(clients_mutex);

    // 找到目标连接后更新昵称；未找到时静默返回，保持调用方逻辑简单。
    for (auto& client : clients) {
        if (client.sock == client_fd) {
            client.name = new_name;
            break;
        }
    }
}

// 判断客户端是否已经完成昵称注册。
//
// 注册状态的用途：
// - false：客户端刚连接，还没有昵称；收到的第一条完整协议消息会被当成昵称。
// - true：客户端已进入聊天室；后续消息按命令或普通聊天内容处理。
bool is_client_registered(int client_fd) {
    std::lock_guard<std::mutex> lock(clients_mutex);

    for (const auto& client : clients) {
        if (client.sock == client_fd) {
            return client.registered;
        }
    }

    return false;
}

// 设置客户端注册状态。
//
// 通常在客户端第一次提交昵称之后由上层调用，将 registered 从 false 改成 true。
// 也可以用于测试或未来扩展中的状态回滚。
void set_client_registered(int client_fd, bool value) {
    std::lock_guard<std::mutex> lock(clients_mutex);

    for (auto& client : clients) {
        if (client.sock == client_fd) {
            client.registered = value;
            break;
        }
    }
}

// 从在线客户端列表中移除指定连接。
//
// 该函数只维护业务层的 clients 列表，不负责：
// - 从 epoll 中删除 fd。
// - close(fd) 释放系统资源。
// 这些 socket/epoll 资源清理由 event_handler 中的 disconnect_client 统一完成。
void remove_client(int client_fd) {
    std::lock_guard<std::mutex> lock(clients_mutex);

    // find_if 根据 socket fd 定位客户端记录。
    auto it = std::find_if(clients.begin(), clients.end(), [client_fd](const ClientInfo& client) {
        return client.sock == client_fd;
    });

    // 找到后 erase 会删除整个 ClientInfo，包括昵称、注册状态和残留接收缓冲区。
    if (it != clients.end()) {
        clients.erase(it);
    }
}

// 获取当前所有“正式在线用户”的昵称列表。
//
// 只返回 registered == true 的客户端，因为刚连接但还没发昵称的连接还不能算聊天室用户。
// /users 命令会使用这个函数生成在线列表。
std::vector<std::string> get_online_users() {
    std::lock_guard<std::mutex> lock(clients_mutex);

    std::vector<std::string> online_users;
    for (const auto& client : clients) {
        if (client.registered) {
            online_users.push_back(client.name);
        }
    }
    return online_users;
}

// 把 socket 本次收到的原始字节追加到该客户端的接收缓冲区。
//
// TCP 是字节流协议，不保证一次 recv 正好对应一条应用层消息：
// - 一条消息可能被拆成多次 recv。
// - 多条消息也可能在一次 recv 中一起到达。
//
// 因此 event_handler 读取到 bytes 后先调用本函数累积数据，再调用
// extract_message_from_client_buffer 按“4 字节长度前缀 + 消息体”的协议拆包。
//
// 返回 true 表示找到了对应客户端并完成追加；返回 false 表示 fd 已不存在或未登记。
bool append_to_client_buffer(int client_fd, const char* data, size_t len) {
    std::lock_guard<std::mutex> lock(clients_mutex);

    for (auto& client : clients) {
        if (client.sock == client_fd) {
            // std::string 也可以保存包含 '\0' 的二进制字节，因此适合作为简单接收缓冲区。
            client.recv_buffer.append(data, len);
            return true;
        }
    }

    return false;
}

// 尝试从指定客户端的接收缓冲区中提取一条完整消息。
//
// 协议格式：
//   [4 字节网络字节序 uint32_t 长度][长度对应的消息体字节]
//
// 例如消息体 "hello" 的长度是 5，缓冲区中会先有 4 字节的 htonl(5)，
// 后面紧跟 5 字节正文。这样服务端就能在 TCP 字节流中判断消息边界。
//
// 参数 msg：
// - 成功提取时写入消息体内容，不包含 4 字节长度头。
// - 数据不完整、消息过大或客户端不存在时保持调用方可忽略的状态。
//
// 返回 true 表示成功取出一条完整消息；返回 false 表示当前暂时没有可处理的完整消息。
bool extract_message_from_client_buffer(int client_fd, std::string& msg) {
    std::lock_guard<std::mutex> lock(clients_mutex);

    // 防止异常客户端声明一个超大长度，导致服务端等待/累积过多内存。
    // 当前单条聊天消息最大允许 4096 字节。
    constexpr uint32_t kMaxMessageSize = 4096;

    for (auto& client : clients) {
        if (client.sock == client_fd) {
            // 还没有凑齐 4 字节长度头，无法知道消息体大小，继续等待后续 recv。
            if (client.recv_buffer.size() < sizeof(uint32_t)) {
                return false;
            }

            // 从缓冲区开头读取网络字节序的长度头。
            // 使用 memcpy 而不是强制指针转换，可以避免未对齐访问带来的未定义行为。
            uint32_t net_len = 0;
            std::memcpy(&net_len, client.recv_buffer.data(), sizeof(net_len));

            // 将网络字节序转换为主机字节序，得到消息体实际长度。
            uint32_t body_len = ntohl(net_len);

            // 长度超过上限说明协议数据异常。这里返回 false，不消费缓冲区；
            // 上层当前会停止本轮处理，未来可以扩展为直接断开该客户端。
            if (body_len > kMaxMessageSize) {
                return false;
            }

            // 长度头已经有了，但消息体还没接收完整，继续等待更多数据。
            if (client.recv_buffer.size() < sizeof(uint32_t) + body_len) {
                return false;
            }

            // 截取完整消息体，并从缓冲区中移除“长度头 + 消息体”。
            // 如果缓冲区后面已经粘着下一条消息，它会被保留下来，供下一次循环继续提取。
            msg = client.recv_buffer.substr(sizeof(uint32_t), body_len);
            client.recv_buffer.erase(0, sizeof(uint32_t) + body_len);

            return true;
        }
    }

    return false;
}
