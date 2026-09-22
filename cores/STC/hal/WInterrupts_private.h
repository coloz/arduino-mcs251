#ifndef STC_WINTERRUPTS_PRIVATE_H
#define STC_WINTERRUPTS_PRIVATE_H

extern void (* volatile stc_external_callbacks[2])(void);
extern volatile uint8_t stc_external_modes[2];

#endif
