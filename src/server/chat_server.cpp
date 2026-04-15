#include"server/chat_server.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include<thread>
#include <algorithm>
#include <ctime>
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

void handle_client(int client_fd) {
    char buffer[1025];
    int n = recv(client_fd, buffer, 1024, 0);

    if (n <= 0) {
        perror("recv");
        remove_client(client_fd);
        close(client_fd);
        return;
    }

    buffer[n] = '\0';
    std::string name = buffer;

    if (name.empty()) {
        name = "unknown";
    }

set_client_name(client_fd, name);

std::string now = get_current_time();
std::string welcome_msg = "[" + now + "] [system] Welcome, " + name + "!";
std::string join_msg = "[" + now + "] [system] " + name + " joined the chat";

send(client_fd, welcome_msg.c_str(), welcome_msg.size(), 0);
std::cout << join_msg << std::endl;
broadcast_msg(client_fd, join_msg);

    while (true) {
        n = recv(client_fd, buffer, 1024, 0);

        if (n == 0) {
            std::string now = get_current_time();
            std::string leave_msg = "[" + now + "] [system] " + name + " left the chat";
            std::cout << leave_msg << std::endl;
            broadcast_msg(client_fd, leave_msg);
            remove_client(client_fd);
            close(client_fd);
            return;
        }

        if (n < 0) {
            perror("recv");
            std::string now = get_current_time();
            std::string leave_msg = "[" + now + "] [system] " + name + " left the chat";
            std::cout << leave_msg << std::endl;
            broadcast_msg(client_fd, leave_msg);
            remove_client(client_fd);
            close(client_fd);
            return;
        }

        buffer[n] = '\0';
        std::string formatted_msg = "[" + get_current_time() + "] [" + name + "]: " + buffer;
        std::cout << "thread " << std::this_thread::get_id()
                  << " " << formatted_msg << std::endl;
        broadcast_msg(client_fd, formatted_msg);
    }
}
