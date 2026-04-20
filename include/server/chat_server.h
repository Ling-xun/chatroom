#pragma once
#include <string>

// 向除发送者之外的所有在线客户端广播一条消息。
void broadcast_msg(int sender_fd, const std::string& msg);

// 获取当前本地时间，格式为 HH:MM:SS。
std::string get_current_time();
