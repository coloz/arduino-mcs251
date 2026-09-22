#ifndef STC_CORE_SFR_H
#define STC_CORE_SFR_H

#include "stc_family.h"

#if !defined(__SDCC)
# error "The arduino-mcs251 core currently requires SDCC"
#endif

/* Common 8051/STC special-function registers used by this minimal core. */
#define STC_SFR(name, address) __sfr __at (address) name

STC_SFR(P0,   0x80);
STC_SFR(PCON, 0x87);
STC_SFR(TCON, 0x88);
STC_SFR(TMOD, 0x89);
STC_SFR(TL0,  0x8a);
STC_SFR(TL1,  0x8b);
STC_SFR(TH0,  0x8c);
STC_SFR(TH1,  0x8d);
STC_SFR(AUXR, 0x8e);
STC_SFR(INTCLKO, 0x8f);
STC_SFR(P1,   0x90);
STC_SFR(SCON, 0x98);
STC_SFR(SBUF, 0x99);
STC_SFR(P2,   0xa0);
STC_SFR(IE,   0xa8);
STC_SFR(IE2,  0xaf);
STC_SFR(T2H,  0xd6);
STC_SFR(T2L,  0xd7);
STC_SFR(AUXINTIF, 0xef);
STC_SFR(P3,   0xb0);

#if STC_CORE_HAS_PORT4
STC_SFR(P4, 0xc0);
#endif
#if STC_CORE_HAS_PORT5
STC_SFR(P5, 0xc8);
#endif
#if STC_CORE_HAS_PORT6
STC_SFR(P6, 0xe8);
#endif
#if STC_CORE_HAS_PORT7
STC_SFR(P7, 0xf8);
#endif

/*
 * STC32G144K246 maps P8/P9/PA/PB into the 24-bit XFR address space.
 * The mcs251 backend emits DPX-assisted accesses for these lvalues.
 */
#define STC_XFR8(address) \
    (*(volatile __xdata unsigned char *)(address))

#if STC_CORE_HAS_PORT8
# define P8OUT  STC_XFR8(0x7ef700UL)
# define P8IN   STC_XFR8(0x7ef708UL)
# define P8M0   STC_XFR8(0x7ef710UL)
# define P8M1   STC_XFR8(0x7ef718UL)
# define P8SETB STC_XFR8(0x7ef720UL)
# define P8CLRB STC_XFR8(0x7ef728UL)
#endif
#if STC_CORE_HAS_PORT9
# define P9OUT  STC_XFR8(0x7ef701UL)
# define P9IN   STC_XFR8(0x7ef709UL)
# define P9M0   STC_XFR8(0x7ef711UL)
# define P9M1   STC_XFR8(0x7ef719UL)
# define P9SETB STC_XFR8(0x7ef721UL)
# define P9CLRB STC_XFR8(0x7ef729UL)
#endif
#if STC_CORE_HAS_PORTA
# define PAOUT  STC_XFR8(0x7ef702UL)
# define PAIN   STC_XFR8(0x7ef70aUL)
# define PAM0   STC_XFR8(0x7ef712UL)
# define PAM1   STC_XFR8(0x7ef71aUL)
# define PASETB STC_XFR8(0x7ef722UL)
# define PACLRB STC_XFR8(0x7ef72aUL)
#endif
#if STC_CORE_HAS_PORTB
# define PBOUT  STC_XFR8(0x7ef703UL)
# define PBIN   STC_XFR8(0x7ef70bUL)
# define PBM0   STC_XFR8(0x7ef713UL)
# define PBM1   STC_XFR8(0x7ef71bUL)
# define PBSETB STC_XFR8(0x7ef723UL)
# define PBCLRB STC_XFR8(0x7ef72bUL)
#endif

STC_SFR(P_SW1, 0xa2);

#if (STC_CORE_HAS_ADC && \
     (STC_CORE_ADC_LAYOUT == STC_ADC_LAYOUT_MODERN_BC_ADCCFG)) || \
    STC_CORE_HAS_PORT8 || STC_CORE_HAS_PORT9 || \
    STC_CORE_HAS_PORTA || STC_CORE_HAS_PORTB || STC_CORE_PWM_LAYOUT || \
    STC_CORE_WIRE_LAYOUT
STC_SFR(P_SW2, 0xba);
#endif

#if STC_CORE_MEMORY_TIMING_LAYOUT || STC_CORE_HAS_PORT8 || STC_CORE_HAS_PORT9 || \
    STC_CORE_HAS_PORTA || STC_CORE_HAS_PORTB
STC_SFR(WTST,  0xe9);
STC_SFR(CKCON, 0xea);
#endif
#if STC_CORE_HAS_PORT8 || STC_CORE_HAS_PORT9 || \
    STC_CORE_HAS_PORTA || STC_CORE_HAS_PORTB
# define TM0PS STC_XFR8(0x7efea0UL)
#endif

#if STC_CORE_HAS_SEPARATE_PULLUP
/* P0-P7 and P8-PB use two XFR banks for their pull-up enables. */
# define P0PU STC_XFR8(0x7efe10UL)
# define P1PU STC_XFR8(0x7efe11UL)
# define P2PU STC_XFR8(0x7efe12UL)
# define P3PU STC_XFR8(0x7efe13UL)
# define P4PU STC_XFR8(0x7efe14UL)
# define P5PU STC_XFR8(0x7efe15UL)
# define P6PU STC_XFR8(0x7efe16UL)
# define P7PU STC_XFR8(0x7efe17UL)
# define P8PU STC_XFR8(0x7ef9c0UL)
# define P9PU STC_XFR8(0x7ef9c1UL)
# define PAPU STC_XFR8(0x7ef9c2UL)
# define PBPU STC_XFR8(0x7ef9c3UL)
#endif

#if STC_CORE_HAS_PORT_MODE
STC_SFR(P1M1, 0x91);
STC_SFR(P1M0, 0x92);
STC_SFR(P0M1, 0x93);
STC_SFR(P0M0, 0x94);
STC_SFR(P2M1, 0x95);
STC_SFR(P2M0, 0x96);
STC_SFR(P3M1, 0xb1);
STC_SFR(P3M0, 0xb2);
STC_SFR(P4M1, 0xb3);
STC_SFR(P4M0, 0xb4);
# if STC_CORE_HAS_PORT5
STC_SFR(P5M1, 0xc9);
STC_SFR(P5M0, 0xca);
# endif
# if STC_CORE_HAS_PORT6
STC_SFR(P6M1, 0xcb);
STC_SFR(P6M0, 0xcc);
# endif
# if STC_CORE_HAS_PORT7
STC_SFR(P7M1, 0xe1);
STC_SFR(P7M0, 0xe2);
# endif
#endif

#if STC_CORE_HAS_ADC
STC_SFR(ADC_CONTR, 0xbc);
STC_SFR(ADC_RES,   0xbd);
STC_SFR(ADC_RESL,  0xbe);
STC_SFR(ADCCFG, 0xde);
#endif


#undef STC_SFR

#define STC_SCON_RI       0x01u
#define STC_SCON_TI       0x02u
#define STC_SCON_REN      0x10u
#define STC_SCON_MODE1    0x40u
#define STC_TCON_TR1      0x40u
#define STC_TCON_TF0      0x20u
#define STC_TCON_TR0      0x10u
#define STC_TCON_IE1      0x08u
#define STC_TCON_IT1      0x04u
#define STC_TCON_IE0      0x02u
#define STC_TCON_IT0      0x01u
#define STC_PCON_SMOD     0x80u
#define STC_AUXR_T0_1T    0x80u
#define STC_AUXR_T1_1T    0x40u
#define STC_AUXR_S1_BRT_T2 0x01u
#define STC_P_SW2_EAXFR   0x80u

#define STC_IE_EA         0x80u
#define STC_IE_ES         0x10u
#define STC_IE_ET1        0x08u
#define STC_IE_EX1        0x04u
#define STC_IE_ET0        0x02u
#define STC_IE_EX0        0x01u

#endif
