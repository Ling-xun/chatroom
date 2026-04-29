#include <sys/socket.h>

#include <string>
#include <vector>

#include "server/chat_server.h"
#include "server/client_manager.h"
#include "server/command_handler.h"

namespace {

// 修改昵称命令的固定前缀，后面跟新的用户名。
const std::string kRenamePrefix = "/rename ";

// 向指定客户端发送一条已经拼好的文本。
// 当前项目所有服务端提示都直接通过 socket 写回客户端。
void send_to_client(int client_fd, const std::string& msg) {
    send(client_fd, msg.c_str(), msg.size(), 0);
}

}  // namespace

bool handle_command(int client_fd, const std::string& msg) {
    // /users：只回复当前客户端，展示所有已经注册昵称的在线用户。
    if (msg == "/users") {
        std::vector<std::string> online_users = get_online_users();

        std::string users_msg = "[" + get_current_time() + "] [system] Online users: ";

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
        std::string new_name = msg.substr(kRenamePrefix.size());

        if (new_name.empty()) {
            std::string err =
                "[" + get_current_time() + "] [system] Usage: /rename <new_name>\n";
            send_to_client(client_fd, err);
            return true;
        }

        std::string old_name = get_client_name(client_fd);
        set_client_name(client_fd, new_name);

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
