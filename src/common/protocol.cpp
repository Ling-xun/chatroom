#include "common/protocol.h"

#include <arpa/inet.h>

#include <cstring>

std::string pack_protocol_message(const std::string& msg) {
    uint32_t body_len = static_cast<uint32_t>(msg.size());
    uint32_t net_len = htonl(body_len);

    std::string packet;
    packet.append(reinterpret_cast<const char*>(&net_len), sizeof(net_len));
    packet.append(msg);
    return packet;
}

ProtocolExtractResult extract_protocol_message(std::string& buffer,
                                               std::string& msg,
                                               uint32_t max_message_size) {
    if (buffer.size() < sizeof(uint32_t)) {
        return ProtocolExtractResult::NeedMoreData;
    }

    uint32_t net_len = 0;
    std::memcpy(&net_len, buffer.data(), sizeof(net_len));

    uint32_t body_len = ntohl(net_len);
    if (body_len > max_message_size) {
        return ProtocolExtractResult::ProtocolError;
    }

    if (buffer.size() < sizeof(uint32_t) + body_len) {
        return ProtocolExtractResult::NeedMoreData;
    }

    msg = buffer.substr(sizeof(uint32_t), body_len);
    buffer.erase(0, sizeof(uint32_t) + body_len);
    return ProtocolExtractResult::Message;
}
