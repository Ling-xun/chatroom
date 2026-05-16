#pragma once

#include <cstdint>
#include <string>

// protocol.h
//
// 聊天室统一使用 4 字节网络字节序长度头 + 消息体的帧协议。

constexpr uint32_t kMaxProtocolMessageSize = 4096;

enum class ProtocolExtractResult {
    Message,
    NeedMoreData,
    ProtocolError
};

std::string pack_protocol_message(const std::string& msg);

ProtocolExtractResult extract_protocol_message(std::string& buffer,
                                               std::string& msg,
                                               uint32_t max_message_size = kMaxProtocolMessageSize);
