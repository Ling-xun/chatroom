#pragma once

#include <string>

// chat_server.h
//
// 声明服务端通用聊天室工具函数：
// - broadcast_msg 负责向其他在线客户端分发消息。
// - get_current_time 负责提供统一时间戳文本。

// 向除发送者之外的所有在线客户端广播一条消息。
//
// epoll_fd 用于在客户端发送缓冲区有数据时开启 EPOLLOUT 事件。
// sender_fd 表示消息来源，广播时会跳过这个 fd，避免发送者重复收到自己的消息。
void broadcast_msg(int epoll_fd, int sender_fd, const std::string& msg);

// 将一条消息加入指定客户端发送缓冲区，并开启该 fd 的 EPOLLOUT 监听。
bool send_msg_to_client(int epoll_fd, int client_fd, const std::string& msg);

// 获取当前本地时间，格式为 HH:MM:SS。
std::string get_current_time();
