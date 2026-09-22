#ifndef STC_ISR_CONTEXT_H
#define STC_ISR_CONTEXT_H

/*
 * SDCC MCS251 ABI revision 2 uses DPXL as the high byte of every flat
 * pointer and DR24/DR28 as code-generator scratch registers.  The pinned
 * stock compiler's interrupt prologue saves DPL/DPH, but omits these extended
 * registers.  An interrupt may therefore corrupt a flat pointer or an
 * indirect-call target owned by the interrupted code.
 *
 * Keep these operations at the first and last C statements of every ISR.
 * The compiler-generated pushes precede ENTER and its pops follow LEAVE, so
 * the interrupted register values are restored before RETI.  This is a core
 * safety net; the matching compiler fix remains required for third-party
 * interrupt functions that do not use this header.  A patched compiler
 * advertises backend-owned preservation with the capability macro below, in
 * which case emitting the source fallback as well would double-save state.
 */
#if defined(__SDCC_mcs251) && \
    !defined(__SDCC_MCS251_EXTENDED_ISR_CONTEXT__)
# define STC_ISR_CONTEXT_ENTER() \
    __asm \
        push dpxl \
        push dr24 \
        push dr28 \
    __endasm
# define STC_ISR_CONTEXT_LEAVE() \
    __asm \
        pop dr28 \
        pop dr24 \
        pop dpxl \
    __endasm
#else
# define STC_ISR_CONTEXT_ENTER() do { } while (0)
# define STC_ISR_CONTEXT_LEAVE() do { } while (0)
#endif

#endif
