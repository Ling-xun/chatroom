#pragma once

#include <cstddef>
#include <string>

// chat_client.h
//
// 声明客户端网络收发接口：
// - send_all 负责完整发送一段字节。
// - send_message 负责按聊天室协议发送一条消息。
// - recv_messages 负责持续接收并打印服务端广播。

// 按长度前缀协议发送一条聊天消息。
bool send_message(int sock, const std::string& msg);

// 接收消息线程：持续接收服务器广播并输出到终端。
//
// 当服务器关闭连接或 recv 失败时，该函数会结束循环并返回。
void recv_messages(int sock);

// 循环调用 send，直到指定字节全部发送完成或连接出错。
bool send_all(int sock, const char* data, size_t len);
