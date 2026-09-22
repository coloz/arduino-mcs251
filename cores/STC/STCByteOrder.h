#ifndef STC_BYTE_ORDER_H
#define STC_BYTE_ORDER_H

#include <stdint.h>

/* Wire/file byte order is explicit and independent of the MCS251 ABI.
 * Byte accesses also support unaligned buffers. The caller supplies at least
 * 2, 4 or 8 accessible bytes; these helpers do not serialize struct padding.
 */
static inline uint16_t stcReadLE16(const void *buffer)
{
    const uint8_t *p = (const uint8_t *)buffer;
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static inline uint16_t stcReadBE16(const void *buffer)
{
    const uint8_t *p = (const uint8_t *)buffer;
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static inline uint32_t stcReadLE32(const void *buffer)
{
    const uint8_t *p = (const uint8_t *)buffer;
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline uint32_t stcReadBE32(const void *buffer)
{
    const uint8_t *p = (const uint8_t *)buffer;
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static inline uint64_t stcReadLE64(const void *buffer)
{
    const uint8_t *p = (const uint8_t *)buffer;
    return (uint64_t)stcReadLE32(p) | ((uint64_t)stcReadLE32(p + 4) << 32);
}

static inline uint64_t stcReadBE64(const void *buffer)
{
    const uint8_t *p = (const uint8_t *)buffer;
    return ((uint64_t)stcReadBE32(p) << 32) | (uint64_t)stcReadBE32(p + 4);
}

static inline void stcWriteLE16(void *buffer, uint16_t value)
{
    uint8_t *p = (uint8_t *)buffer;
    p[0] = (uint8_t)value; p[1] = (uint8_t)(value >> 8);
}

static inline void stcWriteBE16(void *buffer, uint16_t value)
{
    uint8_t *p = (uint8_t *)buffer;
    p[0] = (uint8_t)(value >> 8); p[1] = (uint8_t)value;
}

static inline void stcWriteLE32(void *buffer, uint32_t value)
{
    uint8_t *p = (uint8_t *)buffer;
    p[0] = (uint8_t)value; p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16); p[3] = (uint8_t)(value >> 24);
}

static inline void stcWriteBE32(void *buffer, uint32_t value)
{
    uint8_t *p = (uint8_t *)buffer;
    p[0] = (uint8_t)(value >> 24); p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8); p[3] = (uint8_t)value;
}

static inline void stcWriteLE64(void *buffer, uint64_t value)
{
    uint8_t *p = (uint8_t *)buffer;
    stcWriteLE32(p, (uint32_t)value); stcWriteLE32(p + 4, (uint32_t)(value >> 32));
}

static inline void stcWriteBE64(void *buffer, uint64_t value)
{
    uint8_t *p = (uint8_t *)buffer;
    stcWriteBE32(p, (uint32_t)(value >> 32)); stcWriteBE32(p + 4, (uint32_t)value);
}

#endif
