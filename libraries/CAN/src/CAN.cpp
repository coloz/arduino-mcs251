/* SPDX-License-Identifier: MIT */
#include "CAN.h"
CANClass CAN(0);
CANClass CAN1(1);

int CANClass::write(const CanMsg &message)
{
    stc_can_frame frame;
    frame.id = message.id;
    frame.length = message.data_length;
    for (uint8_t i = 0; i < 8; ++i) frame.data[i] = message.data[i];
    return stc_can_write(_bus, &frame);
}
bool CANClass::read(CanMsg &message)
{
    stc_can_frame frame;
    if (!stc_can_read(_bus, &frame)) return false;
    message.id = frame.id;
    message.data_length = frame.length;
    for (uint8_t i = 0; i < 8; ++i) message.data[i] = frame.data[i];
    return true;
}
size_t CanMsg::printTo(Print &out) const
{
    const char digits[] = "0123456789ABCDEF";
    size_t n = out.write('[');
    uint32_t value = isExtendedId() ? getExtendedId() : getStandardId();
    for (int8_t i = isExtendedId() ? 7 : 2; i >= 0; --i)
        n += out.write((uint8_t)digits[(value >> (i * 4)) & 15]);
    n += out.print("] ("); n += out.print(data_length); n += out.print(") : ");
    if (isRemoteFrame()) return n + out.print("RTR");
    for (uint8_t i = 0; i < data_length && i < 8; ++i) {
        n += out.write((uint8_t)digits[data[i] >> 4]);
        n += out.write((uint8_t)digits[data[i] & 15]);
    }
    return n;
}
