#include "client/chat_client.h"
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include<thread>
#include <cstdio>
#include <iostream>
#include <string>
int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
     if(sock<0){
        perror("socket");
        return -1;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if(connect(sock, (sockaddr*)&addr, sizeof(addr))<0){
        perror("connect");
        close(sock);
        return -1;
    }
    std::string name;
    std::cout<<"请输入您的姓名：";
    getline(std::cin,name);
    if(name.empty()){
        name = "unknown";
    }
    send(sock,name.c_str(),name.size(),0);
    std::thread sender(send_messages,sock);
    std::thread receiver(recv_messages,sock);
    sender.join();
    receiver.join();
    close(sock);
    return 0;
}