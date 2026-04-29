#pragma once

// 处理某个客户端 socket 的可读事件，包括注册、收消息和断开连接。
//
// epoll_fd 用于在连接断开时把客户端 fd 从 epoll 中移除；
// client_fd 是本次触发 EPOLLIN 的客户端连接。
void handle_client_event(int epoll_fd, int client_fd);
