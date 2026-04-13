#include"chat_server.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include<thread>
int main() {

    // 1 创建socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    // 2 定义地址
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;

    // 3 绑定端口
    bind(server_fd, (sockaddr*)&addr, sizeof(addr));

    // 4 开始监听
    listen(server_fd, 5);

    std::cout << "server start at port 8080" << std::endl;
    while(true){

    int client_fd=accept(server_fd,nullptr,nullptr);
    
    if(client_fd<0){
    perror("accept");
    continue;
    }
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        clients.push_back(client_fd);
    }
    std::thread t(handle_client,client_fd);
    t.detach();
   }     
    close(server_fd);
    return 0;
}
