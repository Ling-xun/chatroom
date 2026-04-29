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
            "/help - show command help\n";

        send(client_fd, help_msg.c_str(), help_msg.size(), 0);
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