#include"server/chat_server.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include<thread>
#include <algorithm>
#include <ctime>
#include <sys/epoll.h>
std::vector<ClientInfo> clients;
std::mutex clients_mutex;
std::string get_client_name(int client_fd){
    std::lock_guard<std::mutex> lock(clients_mutex);
    for(const auto&client: clients){
        if(client.sock == client_fd){
            return client.name;
        }
    }
    return "unknown";
}
void set_client_name(int client_fd, const std::string& new_name){
    std::lock_guard<std::mutex> lock(clients_mutex);
    for(auto& client: clients){
        if(client.sock == client_fd){
            client.name = new_name;
            break;
        }
    }
}
std::string get_current_time(){
  time_t now = time(nullptr);
  struct tm* local_time = localtime(&now);
  char time_str[10];
  strftime(time_str, sizeof(time_str), "%H:%M:%S", local_time);
  return std::string(time_str);
}
void broadcast_msg(int sender_fd, const std::string& msg){
    std::vector<ClientInfo> snapshot;

    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        snapshot = clients;
    }
    
    for(const auto&client: snapshot){
        if(client.sock != sender_fd){
            send(client.sock, msg.c_str(), msg.size(), 0);
        }
    }
}
void remove_client(int client_fd){
    std::lock_guard<std::mutex> lock(clients_mutex);
    // 从客户端列表中移除
    auto it = std::find_if(clients.begin(), clients.end(), [client_fd](const ClientInfo& client){
        return client.sock == client_fd;
    });
    if (it != clients.end()) {
        clients.erase(it);
    }
}

bool is_client_registered(int client_fd){
    std::lock_guard<std::mutex> lock(clients_mutex);
    for(const auto&client: clients){
        if(client.sock == client_fd){
            return client.registered;
        }
    }
    return false;
}

void set_client_registered(int client_fd, bool value){
    std::lock_guard<std::mutex> lock(clients_mutex);
    for(auto& client: clients){
        if(client.sock == client_fd){
            client.registered = value;
            break;
        }
    }
}
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
