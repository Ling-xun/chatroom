#include "server/command_handler.h"
#include "server/chat_server.h"
#include "server/client_manager.h"

#include <sys/socket.h>
#include <string>
#include <vector>

bool handle_command(int client_fd, const std::string& msg) {
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
        send(client_fd, users_msg.c_str(), users_msg.size(), 0);
        return true;
    }

    if (msg == "/help") {
        std::string help_msg =
            "[" + get_current_time() + "] [system] Commands:\n"
            "/users - show online users\n"
            "/help - show command help\n"
            "/rename <new_name> - change your name\n";

        send(client_fd, help_msg.c_str(), help_msg.size(), 0);
        return true;
    }
    
    const std::string rename_prefix = "/rename ";

   if (msg.rfind(rename_prefix, 0) == 0) {
    std::string new_name = msg.substr(rename_prefix.size());

    if (new_name.empty()) {
        std::string err =
            "[" + get_current_time() + "] [system] Usage: /rename <new_name>\n";
        send(client_fd, err.c_str(), err.size(), 0);
        return true;
    }

    std::string old_name = get_client_name(client_fd);
    set_client_name(client_fd, new_name);

    std::string self_msg =
        "[" + get_current_time() + "] [system] You changed your name to " + new_name + "\n";
    send(client_fd, self_msg.c_str(), self_msg.size(), 0);

    std::string notify_msg =
        "[" + get_current_time() + "] [system] " + old_name + " changed name to " + new_name + "\n";
    broadcast_msg(client_fd, notify_msg);

    return true;
 }

    if (!msg.empty() && msg[0] == '/') {
        std::string unknown_msg =
            "[" + get_current_time() + "] [system] Unknown command. Type /help for commands.\n";

        send(client_fd, unknown_msg.c_str(), unknown_msg.size(), 0);
        return true;
    }

    return false;
}