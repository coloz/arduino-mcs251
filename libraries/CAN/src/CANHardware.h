/* SPDX-License-Identifier: MIT */
#ifndef STC_CAN_HARDWARE_H
#define STC_CAN_HARDWARE_H
#if STC_CORE_CAN_LAYOUT && defined(__SDCC)
#include <stc_sfr.h>
static __sfr __at (0x97) can_aux;
static __sfr __at (0xa2) can_mux1;
static __sfr __at (0xbb) can_mux2;
static uint8_t can_saved_mux[2], can_saved_high[2];
static uint8_t can_hw_read(uint8_t bus, uint8_t reg)
{
#if STC_CORE_CAN_LAYOUT == 1
    uint8_t value, saved = can_aux & 8u;
    can_aux = (can_aux & (uint8_t)~8u) | (bus ? 8u : 0u);
    STC_XFR8(0x7efebbUL) = reg;
    value = STC_XFR8(0x7efebcUL);
    can_aux = (can_aux & (uint8_t)~8u) | saved;
    return value;
#else
    return STC_XFR8((bus ? 0x7ef300UL : 0x7ef400UL) + reg);
#endif
}
static void can_hw_write(uint8_t bus, uint8_t reg, uint8_t value)
{
#if STC_CORE_CAN_LAYOUT == 1
    uint8_t saved = can_aux & 8u;
    can_aux = (can_aux & (uint8_t)~8u) | (bus ? 8u : 0u);
    STC_XFR8(0x7efebbUL) = reg;
    STC_XFR8(0x7efebcUL) = value;
    can_aux = (can_aux & (uint8_t)~8u) | saved;
#else
    STC_XFR8((bus ? 0x7ef300UL : 0x7ef400UL) + reg) = value;
#endif
}
static void can_hw_enable(uint8_t bus, uint8_t enable, uint8_t route)
{
    uint8_t saved = IE & 0x80u, mask = bus ? 4u : 2u;
    IE &= (uint8_t)~0x80u;
    if (enable) {
        P_SW2 |= 0x80u;
        if (bus) { can_saved_mux[bus] = can_mux2 & 3u; can_mux2 = (can_mux2 & 0xfcu) | (route & 3u); }
        else { can_saved_mux[bus] = can_mux1 & 0x30u; can_mux1 = (can_mux1 & 0xcfu) | ((route & 3u) << 4); }
#if STC_CORE_CAN_LAYOUT == 2
        can_saved_high[bus] = STC_XFR8(0x7efd69UL) & (bus ? 8u : 4u);
        STC_XFR8(0x7efd69UL) = (STC_XFR8(0x7efd69UL) & (uint8_t)~(bus ? 8u : 4u)) |
            ((route & 4u) ? (bus ? 8u : 4u) : 0u);
        can_aux &= (uint8_t)~8u; /* Explicit byte accesses use little-endian register order. */
#endif
        can_aux |= mask;
    } else {
        can_aux &= (uint8_t)~mask;
        if (bus) can_mux2 = (can_mux2 & 0xfcu) | can_saved_mux[bus];
        else can_mux1 = (can_mux1 & 0xcfu) | can_saved_mux[bus];
#if STC_CORE_CAN_LAYOUT == 2
        STC_XFR8(0x7efd69UL) = (STC_XFR8(0x7efd69UL) & (uint8_t)~(bus ? 8u : 4u)) | can_saved_high[bus];
#endif
    }
    IE |= saved;
}
#endif
#endif
