#include"server/chat_server.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ctime>
#include <sys/epoll.h>
#include "server/client_manager.h"
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


