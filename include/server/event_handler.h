#pragma once

// 处理某个客户端 socket 的可读事件，包括注册、收消息和断开连接。
void handle_client_event(int epoll_fd, int client_fd);
