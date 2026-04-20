#include"server/chat_server.h"
#include "server/client_manager.h"
#include "server/event_handler.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <fcntl.h>
int main() {

    // 1. 创建 TCP 监听 socket，作为服务器的入口。
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    // 2. 配置服务器监听地址：监听本机所有网卡的 8080 端口。
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;

    // 3. 将 socket 绑定到指定 IP 和端口。
    bind(server_fd, (sockaddr*)&addr, sizeof(addr));

    // 4. 开始监听客户端连接请求，等待后续 accept。
    listen(server_fd, 5);

    std::cout << "server start at port 8080" << std::endl;

    // 创建 epoll 实例，用于统一管理监听 socket 和所有客户端 socket 的事件。
    int epoll_fd = epoll_create1(0);
    
    if (epoll_fd < 0) {
    perror("epoll_create1");
    close(server_fd);
    return -1;
    }

    epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;

    // 把监听 socket 加入 epoll，后续通过 EPOLLIN 感知新连接到来。
if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) < 0) {
    perror("epoll_ctl: server_fd");
    close(server_fd);
    close(epoll_fd);
    return -1;
}

    // 用于接收本轮 epoll_wait 返回的就绪事件列表。
    epoll_event events[1024];

    // 主事件循环：持续等待新连接或已有客户端发来消息。
    while(true){
        
        int nfds = epoll_wait(epoll_fd, events, 1024, -1);

        if(nfds<0){
        perror("epoll_wait");
        continue;
        }

    for(int i=0;i<nfds;i++){
       if(events[i].data.fd == server_fd){
          // 监听 socket 可读，说明有新的客户端正在建立连接。
          int client_fd=accept(server_fd,nullptr,nullptr);
          if(client_fd<0){
            perror("accept");
            continue;
          }

        {
          // 将新客户端加入全局客户端列表，便于后续广播或状态管理。
          std::lock_guard<std::mutex> lock(clients_mutex);
          clients.push_back({client_fd,"",false});
        }

          epoll_event client_ev;
          client_ev.events = EPOLLIN;
          client_ev.data.fd = client_fd;

          // 把新客户端也注册到 epoll，后续客户端发消息时会进入事件分发逻辑。
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_ev) < 0) {
    perror("epoll_ctl: client_fd");
    close(client_fd);
    remove_client(client_fd);
    continue;
    }
          
}
else{
        // 普通客户端 socket 触发事件，交给专门的事件处理函数处理。
        int client_fd=events[i].data.fd;
        handle_client_event(epoll_fd, client_fd);
    }
}
   
   }     
    close(server_fd);
    return 0;
}
