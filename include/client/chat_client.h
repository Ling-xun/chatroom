#pragma once

#include <cstddef>
#include <string>

bool send_message(int sock, const std::string& msg);

// 接收消息线程：持续接收服务器广播并输出到终端。
//
// 当服务器关闭连接或 recv 失败时，该函数会结束循环并返回。
void recv_messages(int sock);

bool send_all(int sock, const char* data, size_t len);
