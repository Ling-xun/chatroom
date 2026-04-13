#include"chat_server.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include<thread>
#include <algorithm>
std::vector<int> clients;
std::mutex clients_mutex;
void broadcast_msg(int sender_fd, const std::string& msg){
    std::vector<int> snapshot;

    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        snapshot = clients;
    }

    for(int fd : snapshot){
        if(fd != sender_fd){
            send(fd, msg.c_str(), msg.size(), 0);
        }
    }
}
void remove_client(int client_fd){
    std::lock_guard<std::mutex> lock(clients_mutex);
    // 从客户端列表中移除
    auto it = std::find(clients.begin(), clients.end(), client_fd);
    if (it != clients.end()) {
        clients.erase(it);
    }
}
void handle_client(int client_fd){
    
    while(true){
    char buffer[1025];
    int n=recv(client_fd,buffer,1024,0);
    if(n == 0){
    std::cout<<"client disconnected\n";
    remove_client(client_fd);
    close(client_fd);
    return;
    }

    if(n < 0){
    perror("recv");
    remove_client(client_fd);
    close(client_fd);
    return;
    }
    buffer[n] = '\0';
    std::string formatted_msg="["+std::to_string(client_fd)+"]:"+buffer;
    std::cout<<"thread "<<std::this_thread::get_id()
         <<" "<<formatted_msg<<std::endl;
    broadcast_msg(client_fd,formatted_msg);
    }
}

