#pragma once

// 发送消息线程：从标准输入读取内容并发送给服务器。
void send_messages(int sock);

// 接收消息线程：持续接收服务器广播并输出到终端。
void recv_messages(int sock);

