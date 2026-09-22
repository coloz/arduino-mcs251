/* SPDX-License-Identifier: MIT */
#ifndef STC_CAN_BACKEND_H
#define STC_CAN_BACKEND_H
#include <Arduino.h>
#define STC_CAN_OK 0
#define STC_CAN_UNSUPPORTED 1
#define STC_CAN_INVALID 2
#define STC_CAN_BUSY 3
#define STC_CAN_BUS_OFF 4
#define STC_CAN_STOPPED 5
#define STC_CAN_OVERFLOW 6
#define STC_CAN_EFF 0x80000000UL
#define STC_CAN_RTR 0x40000000UL
typedef struct {
    uint32_t id;
    uint8_t length;
    uint8_t data[8];
} stc_can_frame;
#ifdef __cplusplus
extern "C" {
#endif
uint8_t stc_can_begin(uint8_t bus, uint32_t bitrate, uint8_t rx, uint8_t tx) STC_REENTRANT;
void stc_can_end(uint8_t bus) STC_REENTRANT;
int stc_can_write(uint8_t bus, const stc_can_frame *frame) STC_REENTRANT;
uint8_t stc_can_available(uint8_t bus) STC_REENTRANT;
uint8_t stc_can_read(uint8_t bus, stc_can_frame *frame) STC_REENTRANT;
uint8_t stc_can_error(uint8_t bus) STC_REENTRANT;
uint8_t stc_can_filter(uint8_t bus, uint32_t id, uint32_t mask, uint8_t extended) STC_REENTRANT;
#ifdef __cplusplus
}
#endif
#endif
