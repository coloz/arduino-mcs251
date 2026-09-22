/* SPDX-License-Identifier: MIT */
#include "CAN_backend.h"
#include "CANHardware.h"

static uint8_t errors[2];
#if STC_CORE_CAN_LAYOUT
#define QUEUE_SIZE 4u
static uint8_t running[2], head[2], count[2], filtering[2], extended_filter[2];
static uint32_t filter_id[2], filter_mask[2];
static stc_can_frame queue[2][QUEUE_SIZE];

/* Both controllers require exact bit rates. Prefer a sample point near 80%.
 * The classic controller requires TSEG2 >= 2 TQ (encoded value >= 1). */
static uint8_t timing(uint32_t rate, uint8_t *prescale, uint8_t *seg1, uint8_t *seg2)
{
    uint16_t p, n, s2, best = 0xffffu, score;
    uint32_t clocks;
    uint8_t found = 0;
    if (rate < 10000UL || rate > 1000000UL) return 0;
#if STC_CORE_CAN_LAYOUT == 1
    clocks = F_CPU / 2UL;
#else
    clocks = F_CPU;
#endif
    if (clocks % rate) return 0;
    clocks /= rate;
    for (p = 1; p <= (STC_CORE_CAN_LAYOUT == 1 ? 64u : 256u); ++p) {
        if (clocks % p) continue;
        if (clocks / p > (STC_CORE_CAN_LAYOUT == 1 ? 25u : 385u)) continue;
        n = (uint16_t)(clocks / p);
        if (n < 5u) continue;
        for (s2 = 2; s2 <= (STC_CORE_CAN_LAYOUT == 1 ? 8u : 128u); ++s2) {
            if (n <= s2 + 2u || n - s2 - 1u > (STC_CORE_CAN_LAYOUT == 1 ? 16u : 256u)) continue;
            if (STC_CORE_CAN_LAYOUT == 2 && n - s2 < s2 + 2u) continue;
            score = (uint16_t)((s2 * 1000UL) / n);
            score = score > 200u ? score - 200u : 200u - score;
            if (score >= best) continue;
            best = score; *prescale = (uint8_t)(p - 1u);
            *seg1 = (uint8_t)(n - s2 - 2u); *seg2 = (uint8_t)(s2 - 1u); found = 1;
        }
    }
    return found;
}
static uint8_t route_for(uint8_t bus, uint8_t rx, uint8_t tx)
{
    if (!bus) {
        if (rx == 0x00u && tx == 0x01u) return 0;
#if STC_CORE_CAN_LAYOUT == 1
        if (rx == 0x50u && tx == 0x51u) return 1;
#endif
        /* G144 route 1 is omitted: its manual says P5.2 TX, the vendor
         * example/header says P5.1. Use an unambiguous route until verified. */
        if (rx == 0x42u && tx == 0x45u) return 2;
        if (rx == 0x70u && tx == 0x71u) return 3;
#if STC_CORE_CAN_LAYOUT == 2
        if (rx == 0x30u && tx == 0x31u) return 4;
        if (rx == 0x36u && tx == 0x37u) return 5;
        if (rx == 0x14u && tx == 0x15u) return 6;
        if (rx == 0x43u && tx == 0x44u) return 7;
#endif
    } else {
        if (rx == 0x02u && tx == 0x03u) return 0;
        if (rx == 0x52u && tx == 0x53u) return 1;
        if (rx == 0x46u && tx == 0x47u) return 2;
        if (rx == 0x72u && tx == 0x73u) return 3;
#if STC_CORE_CAN_LAYOUT == 2
        if (rx == 0x10u && tx == 0x11u) return 4;
        if (rx == 0x84u && tx == 0x85u) return 5;
        if (rx == 0xa0u && tx == 0xa1u) return 6;
#endif
    }
    return 0xffu;
}
static uint8_t bus_off(uint8_t bus)
{
    if (can_hw_read(bus, STC_CORE_CAN_LAYOUT == 1 ? 2u : 0xa0u) & 1u) {
        errors[bus] = STC_CAN_BUS_OFF; return 1;
    }
    return 0;
}
static uint8_t receive(uint8_t bus, stc_can_frame *f)
{
    uint8_t i, control, n, status;
#if STC_CORE_CAN_LAYOUT == 1
    uint8_t offset;
#endif
    uint32_t id = 0;
    memset(f, 0, sizeof(*f));
#if STC_CORE_CAN_LAYOUT == 1
    status = can_hw_read(bus, 2);
    if (status & 0x40u) { errors[bus] = STC_CAN_OVERFLOW; can_hw_write(bus, 3, 1); }
    if (!(status & 0x80u)) return 0;
    control = can_hw_read(bus, 12); offset = 1;
    n = control & 0x80u ? 4u : 2u;
    for (i = 0; i < n; ++i) id = (id << 8) | can_hw_read(bus, 12u + (offset++ & 3u));
    id >>= control & 0x80u ? 3 : 5;
    n = (control & 15u) > 8u ? 8u : control & 15u;
    if (!(control & 0x40u)) for (i = 0; i < n; ++i) f->data[i] = can_hw_read(bus, 12u + (offset++ & 3u));
    while (offset & 3u) { (void)can_hw_read(bus, 12u + (offset & 3u)); ++offset; }
#else
    status = can_hw_read(bus, 0xa3);
    if (status & 0x20u) errors[bus] = STC_CAN_OVERFLOW;
    if (!(status & 3u)) return 0;
    control = can_hw_read(bus, 4);
    for (i = 0; i < 4; ++i) id |= (uint32_t)can_hw_read(bus, i) << (i * 8);
    n = (control & 15u) > 8u ? 8u : control & 15u;
    if (!(control & 0x40u)) for (i = 0; i < n; ++i) f->data[i] = can_hw_read(bus, 8u + i);
    can_hw_write(bus, 0xa3, 0x50); /* Preserve ROM, release exactly one RX slot. */
    if (control & 0x20u) return 2; /* Classic API never truncates CAN-FD into a classic frame. */
#endif
    f->id = id | ((control & 0x80u) ? STC_CAN_EFF : 0UL) | ((control & 0x40u) ? STC_CAN_RTR : 0UL);
    f->length = n;
    return 1;
}
#endif

uint8_t stc_can_begin(uint8_t bus, uint32_t bitrate, uint8_t rx, uint8_t tx) STC_REENTRANT
{
#if STC_CORE_CAN_LAYOUT
    uint8_t p, s1, s2, route, i;
    if (bus > 1) return 0;
    stc_can_end(bus);
    if (rx == 0xffu && tx == 0xffu) { rx = bus ? 0x02u : 0x00u; tx = rx + 1u; }
    route = route_for(bus, rx, tx);
    if (!digitalPinIsValid(rx) || !digitalPinIsValid(tx) || route == 0xffu || !timing(bitrate, &p, &s1, &s2)) {
        errors[bus] = STC_CAN_INVALID; return 0;
    }
    digitalWrite(tx, HIGH); pinMode(tx, OUTPUT); pinMode(rx, INPUT_PULLUP);
    can_hw_enable(bus, 1, route);
#if STC_CORE_CAN_LAYOUT == 1
    can_hw_write(bus, 0, 4); can_hw_write(bus, 4, 0);
    can_hw_write(bus, 6, p | ((s2 > 3u ? 3u : s2) << 6));
    can_hw_write(bus, 7, (s2 << 4) | s1);
    for (i = 0; i < 4; ++i) { can_hw_write(bus, 16u + i, 0); can_hw_write(bus, 20u + i, 255); }
    can_hw_write(bus, 3, 127); can_hw_write(bus, 0, 1);
#else
    can_hw_write(bus, 0xa0, 0x80);
    can_hw_write(bus, 0xa4, 0); can_hw_write(bus, 0xa6, 0);
    can_hw_write(bus, 0xa8, s1); can_hw_write(bus, 0xa9, s2);
    can_hw_write(bus, 0xaa, s2 > 3u ? 3u : s2); can_hw_write(bus, 0xab, p);
    can_hw_write(bus, 0xb4, 0);
    for (i = 0; i < 4; ++i) can_hw_write(bus, 0xb8u + i, 0);
    can_hw_write(bus, 0xb4, 0x20);
    for (i = 0; i < 4; ++i) can_hw_write(bus, 0xb8u + i, i == 3u ? 0x1fu : 0xffu);
    can_hw_write(bus, 0xb6, 1); can_hw_write(bus, 0xb7, 0);
    can_hw_write(bus, 0xb5, 0); can_hw_write(bus, 0xbf, 0); /* Disable timestamp/TTCAN. */
    can_hw_write(bus, 0xa2, 0x80); can_hw_write(bus, 0xa1, 0);
    can_hw_write(bus, 0xa0, 0); can_hw_write(bus, 0xa3, 0x40);
    can_hw_write(bus, 0xa5, 255);
#endif
    running[bus] = 1; errors[bus] = STC_CAN_OK; return 1;
#else
    (void)bitrate; (void)rx; (void)tx;
    if (bus < 2) errors[bus] = STC_CAN_UNSUPPORTED;
    return 0;
#endif
}
void stc_can_end(uint8_t bus) STC_REENTRANT
{
    if (bus > 1) return;
#if STC_CORE_CAN_LAYOUT
    if (running[bus]) {
        can_hw_write(bus, STC_CORE_CAN_LAYOUT == 1 ? 0u : 0xa0u, STC_CORE_CAN_LAYOUT == 1 ? 4u : 0x80u);
        can_hw_enable(bus, 0, 0);
    }
    running[bus] = head[bus] = count[bus] = filtering[bus] = 0;
#endif
    errors[bus] = STC_CAN_STOPPED;
}
int stc_can_write(uint8_t bus, const stc_can_frame *f) STC_REENTRANT
{
#if STC_CORE_CAN_LAYOUT
    uint8_t i, control;
#if STC_CORE_CAN_LAYOUT == 1
    uint8_t offset, n;
#endif
    uint32_t id;
    if (bus > 1 || !f) return -STC_CAN_INVALID;
    if (!running[bus]) { errors[bus] = STC_CAN_STOPPED; return -STC_CAN_STOPPED; }
    if (f->length > 8 || (f->id & 0x20000000UL) || (!(f->id & STC_CAN_EFF) && (f->id & 0x1ffff800UL))) {
        errors[bus] = STC_CAN_INVALID; return -STC_CAN_INVALID;
    }
    if (bus_off(bus)) return -STC_CAN_BUS_OFF;
#if STC_CORE_CAN_LAYOUT == 1
    if (!(can_hw_read(bus, 2) & 0x20u)) { errors[bus] = STC_CAN_BUSY; return -STC_CAN_BUSY; }
#else
    if (can_hw_read(bus, 0xa1) & 0x10u) { errors[bus] = STC_CAN_BUSY; return -STC_CAN_BUSY; }
#endif
    control = f->length | ((f->id & STC_CAN_EFF) ? 0x80u : 0u) | ((f->id & STC_CAN_RTR) ? 0x40u : 0u);
    id = f->id & 0x1fffffffUL;
#if STC_CORE_CAN_LAYOUT == 1
    can_hw_write(bus, 8, control); offset = 1;
    n = (f->id & STC_CAN_EFF) ? 4u : 2u; id <<= n == 4u ? 3 : 5;
    for (i = n; i; --i) can_hw_write(bus, 8u + (offset++ & 3u), (uint8_t)(id >> ((i - 1u) * 8)));
    if (!(f->id & STC_CAN_RTR)) for (i = 0; i < f->length; ++i) can_hw_write(bus, 8u + (offset++ & 3u), f->data[i]);
    while (offset & 3u) { can_hw_write(bus, 8u + (offset & 3u), 0); ++offset; }
    can_hw_write(bus, 1, 4);
#else
    can_hw_write(bus, 0xa1, 0); /* Select primary TX buffer. */
    for (i = 0; i < 4; ++i) can_hw_write(bus, 0x50u + i, (uint8_t)(id >> (i * 8)));
    can_hw_write(bus, 0x54, control);
    for (i = 5; i < 8; ++i) can_hw_write(bus, 0x50u + i, 0);
    if (!(f->id & STC_CAN_RTR)) for (i = 0; i < f->length; ++i) can_hw_write(bus, 0x58u + i, f->data[i]);
    can_hw_write(bus, 0xa5, 0x08); can_hw_write(bus, 0xa1, 0x10);
#endif
    errors[bus] = STC_CAN_OK; return 1;
#else
    (void)f; if (bus < 2) errors[bus] = STC_CAN_UNSUPPORTED;
    return -STC_CAN_UNSUPPORTED;
#endif
}
uint8_t stc_can_available(uint8_t bus) STC_REENTRANT
{
#if STC_CORE_CAN_LAYOUT
    uint8_t budget = 8, result;
    stc_can_frame *f;
    if (bus > 1 || !running[bus]) return 0;
    if (bus_off(bus)) return count[bus];
    while (count[bus] < QUEUE_SIZE && budget--) {
        f = &queue[bus][(head[bus] + count[bus]) % QUEUE_SIZE];
        result = receive(bus, f);
        if (!result) break;
        if (result != 1) continue;
        if (filtering[bus] && (((f->id & STC_CAN_EFF) != 0) != extended_filter[bus] ||
            ((f->id & filter_mask[bus]) != (filter_id[bus] & filter_mask[bus])))) continue;
        ++count[bus];
    }
    return count[bus];
#else
    (void)bus; return 0;
#endif
}
uint8_t stc_can_read(uint8_t bus, stc_can_frame *f) STC_REENTRANT
{
#if STC_CORE_CAN_LAYOUT
    if (!f || !stc_can_available(bus)) return 0;
    *f = queue[bus][head[bus]]; head[bus] = (head[bus] + 1u) % QUEUE_SIZE; --count[bus]; return 1;
#else
    (void)bus; (void)f; return 0;
#endif
}
uint8_t stc_can_error(uint8_t bus) STC_REENTRANT { return bus < 2 ? errors[bus] : STC_CAN_INVALID; }
uint8_t stc_can_filter(uint8_t bus, uint32_t id, uint32_t mask, uint8_t extended) STC_REENTRANT
{
#if STC_CORE_CAN_LAYOUT
    uint32_t limit = extended ? 0x1fffffffUL : 0x7ffUL;
    if (bus > 1) return 0;
    if (!running[bus]) { errors[bus] = STC_CAN_STOPPED; return 0; }
    if (id > limit || mask > limit) { errors[bus] = STC_CAN_INVALID; return 0; }
    filtering[bus] = 1; extended_filter[bus] = extended != 0;
    filter_id[bus] = id; filter_mask[bus] = mask; count[bus] = head[bus] = 0;
    errors[bus] = STC_CAN_OK; return 1;
#else
    (void)id; (void)mask; (void)extended;
    if (bus < 2) errors[bus] = STC_CAN_UNSUPPORTED; return 0;
#endif
}
