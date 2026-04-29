#pragma once

// 发送消息线程：从标准输入读取内容并发送给服务器。
//
// 用户输入 exit 时会关闭 socket 的写方向，用于主动退出聊天。
void send_messages(int sock);

// 接收消息线程：持续接收服务器广播并输出到终端。
//
// 当服务器关闭连接或 recv 失败时，该函数会结束循环并返回。
void recv_messages(int sock);
