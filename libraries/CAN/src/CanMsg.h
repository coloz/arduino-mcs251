/* SPDX-License-Identifier: MIT */
#ifndef STC_CAN_MSG_H
#define STC_CAN_MSG_H

#include <Arduino.h>

namespace arduino {
class CanMsg : public Printable {
public:
    static constexpr uint8_t MAX_DATA_LENGTH = 8;
    static constexpr uint32_t CAN_EFF_FLAG = 0x80000000UL;
    static constexpr uint32_t CAN_SFF_MASK = 0x7ffUL;
    static constexpr uint32_t CAN_EFF_MASK = 0x1fffffffUL;
    // STC extension: preserves remote frames when using the message API.
    static constexpr uint32_t CAN_RTR_FLAG = 0x40000000UL;

    CanMsg() : id(0), data_length(0), data{} {}
    CanMsg(uint32_t identifier, uint8_t length, const uint8_t *bytes)
        : id(identifier), data_length(length > 8 ? 8 : length), data{} {
        if (bytes) for (uint8_t i = 0; i < data_length; ++i) data[i] = bytes[i];
    }
    uint32_t getStandardId() const { return id & CAN_SFF_MASK; }
    uint32_t getExtendedId() const { return id & CAN_EFF_MASK; }
    bool isStandardId() const { return !(id & CAN_EFF_FLAG); }
    bool isExtendedId() const { return (id & CAN_EFF_FLAG) != 0; }
    bool isRemoteFrame() const { return (id & CAN_RTR_FLAG) != 0; }
    size_t printTo(Print &out) const override;

    uint32_t id;
    uint8_t data_length;
    uint8_t data[MAX_DATA_LENGTH];
};
inline uint32_t CanStandardId(uint32_t id) { return id & CanMsg::CAN_SFF_MASK; }
inline uint32_t CanExtendedId(uint32_t id) { return (id & CanMsg::CAN_EFF_MASK) | CanMsg::CAN_EFF_FLAG; }
}
using arduino::CanMsg;
using arduino::CanStandardId;
using arduino::CanExtendedId;
#endif
