#pragma once

#include <string>

// 解析并执行客户端输入的命令。
//
// 返回 true 表示 msg 已经被当作命令处理完，调用方不应再把它作为普通聊天消息广播；
// 返回 false 表示 msg 不是命令，可以继续走普通聊天消息流程。
bool handle_command(int client_fd, const std::string& msg);
