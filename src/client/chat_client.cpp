#include "client/chat_client.h"
#include <string>
#include<mutex>
#include <iostream>
#include <sys/socket.h>
std::mutex cout_mutex;

// 持续读取用户输入并发给服务器；输入 exit 时关闭发送方向。
void send_messages(int sock){
    while(true){
        std::string msg;
        std::getline(std::cin,msg);
        if(msg.empty()){
        continue;
        }
        if(msg=="exit"){
        shutdown(sock,SHUT_WR);
        break;
        }
        send(sock, msg.c_str(),msg.size(), 0);
    }
}

// 持续接收服务器广播，并串行化终端输出，避免与输入线程互相打断。
void recv_messages(int sock){
    while(true){
        char buffer[1025] ;
        int n=recv(sock, buffer, 1024, 0);
        if(n<=0)break;
        buffer[n] = '\0';
{
    std::lock_guard<std::mutex> lock(cout_mutex);
    std::cout << buffer << std::endl;
}     
    }
}
