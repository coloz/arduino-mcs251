#include <Arduino.h>
#include <STCByteOrder.h>

/* Eight explicit bytes, identical protocol on both CPUs: sync, LE16 sequence,
 * LE32 millis and XOR. The receiver must validate the packet before decoding. */
static uint16_t sequence;
void setup(void) { Serial.begin(115200UL); }
void loop(void) {
    uint8_t packet[8], i;
    packet[0] = 0xa5;
    stcWriteLE16(packet + 1, sequence++);
    stcWriteLE32(packet + 3, (uint32_t)millis());
    packet[7] = 0;
    for (i = 0; i < 7; ++i) packet[7] ^= packet[i];
    for (i = 0; i < 8; ++i) Serial.write(packet[i]);
    delay(100);
}
