#include"server/chat_server.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <fcntl.h>
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

    int epoll_fd = epoll_create1(0);
    
    if (epoll_fd < 0) {
    perror("epoll_create1");
    close(server_fd);
    return -1;
    }

    epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;

if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) < 0) {
    perror("epoll_ctl: server_fd");
    close(server_fd);
    close(epoll_fd);
    return -1;
}

    epoll_event events[1024];

    while(true){
        
        int nfds = epoll_wait(epoll_fd, events, 1024, -1);

        if(nfds<0){
        perror("epoll_wait");
        continue;
        }

    for(int i=0;i<nfds;i++){
       if(events[i].data.fd == server_fd){
          int client_fd=accept(server_fd,nullptr,nullptr);
          if(client_fd<0){
            perror("accept");
            continue;
          }

        {
          std::lock_guard<std::mutex> lock(clients_mutex);
          clients.push_back({client_fd,"",false});
        }

          epoll_event client_ev;
          client_ev.events = EPOLLIN;
          client_ev.data.fd = client_fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_ev) < 0) {
    perror("epoll_ctl: client_fd");
    close(client_fd);
    remove_client(client_fd);
    continue;
    }
          
}
else{
        int client_fd=events[i].data.fd;
        handle_client_event(epoll_fd, client_fd);
    }
}
   
   }     
    close(server_fd);
    return 0;
}
