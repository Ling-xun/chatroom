#include "client/chat_client.h"
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include<thread>
#include <cstdio>
#include <iostream>
#include <string>
int main() {
    // 创建客户端 socket，并准备连接本地聊天服务器。
    int sock = socket(AF_INET, SOCK_STREAM, 0);
     if(sock<0){
        perror("socket");
        return -1;
    }

    // 目标地址固定为本机 8080 端口。
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if(connect(sock, (sockaddr*)&addr, sizeof(addr))<0){
        perror("connect");
        close(sock);
        return -1;
    }

    // 首次输入的昵称会被服务器用作用户注册信息。
    std::string name;
    std::cout<<"请输入您的姓名：";
    getline(std::cin,name);
    std::cout<<"\n";
    if(name.empty()){
        name = "unknown";
    }
    send(sock,name.c_str(),name.size(),0);

    // 分别开启发送线程和接收线程，实现终端输入与消息显示并行进行。
    std::thread sender(send_messages,sock);
    std::thread receiver(recv_messages,sock);
    sender.join();
    receiver.join();

    // 两个线程结束后再关闭 socket，完成客户端退出。
    close(sock);
    return 0;
}
