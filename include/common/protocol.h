#pragma once

#include <cstdint>
#include <string>

// protocol.h
//
// 聊天室统一使用 4 字节网络字节序长度头 + 消息体的帧协议。

// 单条应用层消息的最大正文长度。
//
// 这个限制用于保护服务端和客户端的接收缓冲区，避免异常长度头导致内存无限增长。
constexpr uint32_t kMaxProtocolMessageSize = 4096;

// 从字节流缓冲区提取协议消息时的结果。
enum class ProtocolExtractResult {
    // 已经成功取出一条完整消息，输出参数 msg 可用。
    Message,

    // 当前缓冲区还不够组成一条完整消息，调用方应等待更多数据。
    NeedMoreData,

    // 长度头或消息大小违反协议约束，调用方通常应断开连接。
    ProtocolError
};

// 将一条消息正文打包成可写入 socket 的协议帧。
std::string pack_protocol_message(const std::string& msg);

// 尝试从缓冲区中提取一条完整协议消息。
//
// 成功提取时会从 buffer 中移除已消费的协议帧，并把消息正文写入 msg。
ProtocolExtractResult extract_protocol_message(std::string& buffer,
                                               std::string& msg,
                                               uint32_t max_message_size = kMaxProtocolMessageSize);
