#include <sys/socket.h>

#include <string>
#include <vector>

#include "server/chat_server.h"
#include "server/client_manager.h"
#include "server/command_handler.h"

// command_handler.cpp
//
// 该文件负责识别并执行用户在聊天室里输入的命令。
//
// 命令与普通聊天消息共用同一条输入通道，因此 handle_command 的返回值非常关键：
// - true：消息已经被命令处理消耗，调用方不应再广播。
// - false：消息不是命令，调用方可以继续按普通聊天内容处理。

namespace {

// 修改昵称命令的固定前缀，后面跟新的用户名。
//
// 例如用户输入 "/rename Alice"，前缀匹配后剩余的 "Alice" 会作为新昵称。
const std::string kRenamePrefix = "/rename ";

// 向指定客户端发送一条已经拼好的文本。
//
// 当前项目所有服务端提示都直接通过 socket 写回客户端。该辅助函数只包一层 send，
// 让命令处理主体不用反复写 c_str()/size()。
void send_to_client(int client_fd, const std::string& msg) {
    send(client_fd, msg.c_str(), msg.size(), 0);
}

}  // namespace

// 解析并执行客户端输入的命令。
//
// 当前支持：
// - /users：查看已经完成注册的在线用户。
// - /help：查看命令说明。
// - /rename <new_name>：修改自己的昵称。
//
// 对未知的 "/" 开头文本也返回 true，并给客户端提示“未知命令”，避免用户输错命令时
// 被当作普通聊天内容广播出去。
bool handle_command(int client_fd, const std::string& msg) {
    // /users：只回复当前客户端，展示所有已经注册昵称的在线用户。
    if (msg == "/users") {
        // 在线列表来自 client_manager，只包含 registered == true 的用户。
        std::vector<std::string> online_users = get_online_users();

        std::string users_msg = "[" + get_current_time() + "] [system] Online users: ";

        // 手动拼接逗号分隔列表，最后一个用户后不追加多余逗号。
        for (size_t i = 0; i < online_users.size(); i++) {
            users_msg += online_users[i];
            if (i + 1 < online_users.size()) {
                users_msg += ", ";
            }
        }

        users_msg += "\n";
        send_to_client(client_fd, users_msg);
        return true;
    }

    // /help：返回当前支持的命令列表，方便用户在客户端里查看用法。
    if (msg == "/help") {
        std::string help_msg =
            "[" + get_current_time() + "] [system] Commands:\n"
            "/users - show online users\n"
            "/help - show command help\n"
            "/rename <new_name> - change your name\n";

        send_to_client(client_fd, help_msg);
        return true;
    }

    // /rename <new_name>：更新当前客户端昵称，并通知其他在线用户。
    // rfind(prefix, 0) == 0 表示 msg 以指定前缀开头。
    if (msg.rfind(kRenamePrefix, 0) == 0) {
        // substr 去掉命令前缀后，剩余内容就是用户想设置的新昵称。
        std::string new_name = msg.substr(kRenamePrefix.size());

        if (new_name.empty()) {
            // 只有命令没有参数时，告诉当前用户正确用法；不广播错误输入。
            std::string err =
                "[" + get_current_time() + "] [system] Usage: /rename <new_name>\n";
            send_to_client(client_fd, err);
            return true;
        }

        // 修改前先保存旧昵称，用于向其他用户说明是谁改名了。
        std::string old_name = get_client_name(client_fd);
        set_client_name(client_fd, new_name);

        // 当前用户收到确认消息，其他用户收到改名通知。
        std::string self_msg =
            "[" + get_current_time() + "] [system] You changed your name to " + new_name + "\n";
        send_to_client(client_fd, self_msg);

        std::string notify_msg =
            "[" + get_current_time() + "] [system] " + old_name + " changed name to " + new_name +
            "\n";
        broadcast_msg(client_fd, notify_msg);

        return true;
    }

    // 以 / 开头但不在支持列表里的文本，认为是未知命令。
    // 这样用户输错命令时不会被当作普通聊天消息广播出去。
    if (!msg.empty() && msg[0] == '/') {
        std::string unknown_msg =
            "[" + get_current_time() + "] [system] Unknown command. Type /help for commands.\n";

        send_to_client(client_fd, unknown_msg);
        return true;
    }

    // 返回 false 表示这不是命令，调用方可以继续按普通聊天消息处理。
    return false;
}
