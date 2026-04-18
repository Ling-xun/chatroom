#include"server/chat_server.h"
#include "server/client_manager.h"
#include "server/event_handler.h"
#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/epoll.h>

void handle_client_event(int epoll_fd, int client_fd) {
   char buffer[1025];
   int n = recv(client_fd, buffer, 1024, 0);
   if (n <= 0) {
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
    if (msg.empty()) {
        msg = "unknown";
    }

    set_client_name(client_fd, msg);
    set_client_registered(client_fd, true);

    std::string welcome_msg = "[" + get_current_time() + "] [system] Welcome, " + msg + "!\n";
    send(client_fd, welcome_msg.c_str(), welcome_msg.size(), 0);

    std::string join_msg = "[" + get_current_time() + "] [system] " + msg + " joined the chat\n";
    std::cout << join_msg;
    broadcast_msg(client_fd, join_msg);
}
  else{
    std::string name = get_client_name(client_fd);
    std::string formatted_msg = "[" + get_current_time() + "] [" + name + "]: " + msg + "\n";
    std::cout << formatted_msg;
    broadcast_msg(client_fd, formatted_msg);
  }
}
