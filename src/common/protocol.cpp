#include "common/protocol.h"

#include <arpa/inet.h>

#include <cstring>

// protocol.cpp
//
// 该文件只负责聊天室的应用层帧协议：
// [4 字节网络字节序 uint32_t 正文长度][正文 bytes]
// TCP 本身没有消息边界，所以客户端和服务端都通过这里的函数统一封包/拆包。

// 将一条文本消息转换成协议帧。
//
// 长度头使用网络字节序，保证不同 CPU 字节序的机器之间也能正确通信。
std::string pack_protocol_message(const std::string& msg) {
    uint32_t body_len = static_cast<uint32_t>(msg.size());
    uint32_t net_len = htonl(body_len);

    std::string packet;
    packet.append(reinterpret_cast<const char*>(&net_len), sizeof(net_len));
    packet.append(msg);
    return packet;
}

// 从累计接收缓冲区中尝试拆出一条消息。
//
// buffer 可能只含半条消息，也可能含多条消息；本函数最多提取一条，调用方可以循环调用。
ProtocolExtractResult extract_protocol_message(std::string& buffer,
                                               std::string& msg,
                                               uint32_t max_message_size) {
    // 长度头都没收齐时，无法判断正文大小，继续等待下一次 recv。
    if (buffer.size() < sizeof(uint32_t)) {
        return ProtocolExtractResult::NeedMoreData;
    }

    uint32_t net_len = 0;
    std::memcpy(&net_len, buffer.data(), sizeof(net_len));

    uint32_t body_len = ntohl(net_len);
    if (body_len > max_message_size) {
        return ProtocolExtractResult::ProtocolError;
    }

    // 正文还没收齐时不能消费缓冲区，否则半包数据会丢失。
    if (buffer.size() < sizeof(uint32_t) + body_len) {
        return ProtocolExtractResult::NeedMoreData;
    }

    // 成功拆包后移除已消费的帧，保留后面可能已经到达的下一条消息。
    msg = buffer.substr(sizeof(uint32_t), body_len);
    buffer.erase(0, sizeof(uint32_t) + body_len);
    return ProtocolExtractResult::Message;
}
