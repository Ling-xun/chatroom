#pragma once

#include <cstdint>

// event_handler.h
//
// 声明服务端客户端事件处理入口，用于处理 epoll 检测到的客户端可读事件。

// 处理某个客户端 socket 的 epoll 事件，包括收消息、发送缓冲区落盘和断开连接。
//
// epoll_fd 用于在连接断开时把客户端 fd 从 epoll 中移除；
// client_fd 是本次触发 EPOLLIN 的客户端连接。
void handle_client_event(int epoll_fd, int client_fd, uint32_t events);
