#ifndef STCXX_RUNTIME_H
#define STCXX_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

/*
 * This header is the C ABI between startup, the C++ runtime, and the generated
 * constructor bridge.  Keep it valid C: main.c deliberately remains a C
 * translation unit even when the C++ profile is selected.
 */
#ifdef __cplusplus
extern "C" {
#endif

#define STCXX_RUNTIME_ABI_VERSION 1u
#define STCXX_GUARD_LAYOUT_VERSION 1u
#define STCXX_GUARD_SIZE 1u

#ifndef STCXX_RUNTIME_HOST_TEST
# define STCXX_RUNTIME_HOST_TEST 0
#endif
#if defined(STCXX_TARGET_MCS51) && STCXX_TARGET_MCS51
# error "MCS51 C++ support has been removed"
#endif
#ifndef STCXX_TARGET_MCS251
# define STCXX_TARGET_MCS251 0
#endif
#ifndef STCXX_TARGET_ENDIAN_LITTLE
# define STCXX_TARGET_ENDIAN_LITTLE 0
#endif
#ifndef STCXX_TARGET_ENDIAN_BIG
# define STCXX_TARGET_ENDIAN_BIG 0
#endif

#if ((STCXX_RUNTIME_HOST_TEST != 0) && (STCXX_RUNTIME_HOST_TEST != 1)) || \
    ((STCXX_TARGET_MCS251 != 0) && (STCXX_TARGET_MCS251 != 1)) || \
    ((STCXX_TARGET_ENDIAN_LITTLE != 0) && \
     (STCXX_TARGET_ENDIAN_LITTLE != 1)) || \
    ((STCXX_TARGET_ENDIAN_BIG != 0) && (STCXX_TARGET_ENDIAN_BIG != 1))
# error "STC C++ runtime switches must be numeric 0 or 1"
#endif

/*
 * -fno-threadsafe-statics makes Clang allocate and access one guard byte
 * directly.  No __cxa_guard_* calls are part of this ABI.
 */
typedef uint8_t stcxx_guard_t;

typedef enum stcxx_ctor_state {
    STCXX_CTORS_NOT_STARTED = 0,
    STCXX_CTORS_RUNNING = 1,
    STCXX_CTORS_COMPLETE = 2
} stcxx_ctor_state_t;

typedef enum stcxx_panic_reason {
    STCXX_PANIC_CTOR_REENTRY = 1,
    STCXX_PANIC_CTOR_BRIDGE_RANGE = 2,
    STCXX_PANIC_PURE_VIRTUAL = 3,
    STCXX_PANIC_DELETED_VIRTUAL = 4,
    STCXX_PANIC_OUT_OF_MEMORY = 5
} stcxx_panic_reason_t;

typedef void (*stcxx_panic_hook_t)(stcxx_panic_reason_t reason);
typedef void (*stcxx_oom_hook_t)(size_t requested_size);

/*
 * These symbols identify only the runtime sub-ABI (guard and pointer-sized
 * interfaces).  The build profile must additionally reject differences in
 * memory model, stack-auto, xstack, floating model, address spaces and
 * exceptions/RTTI before link.  A generated bridge calls this runtime probe
 * as one part of that complete build-identity check.
 */
#if STCXX_RUNTIME_HOST_TEST
# define STCXX_ABI_IDENTITY_TEXT \
    "stc-arduino-cxx-v1-host-test-guard1-direct"
# define STCXX_ABI_IDENTITY_SYMBOL \
    __stcxx_abi_stc_arduino_cxx_v1_host_test_guard1_direct
#elif STCXX_TARGET_MCS251
# if !STCXX_TARGET_ENDIAN_BIG || STCXX_TARGET_ENDIAN_LITTLE
#  error "MCS251 runtime identity requires STCXX_TARGET_ENDIAN_BIG"
# endif
# define STCXX_ABI_IDENTITY_TEXT \
    "stc-arduino-cxx-v1-mcs251-be-size4-pdiff4-ptr3-fnptr3-guard1-direct"
# define STCXX_ABI_IDENTITY_SYMBOL \
    __stcxx_abi_stc_arduino_cxx_v1_mcs251_be_size4_pdiff4_ptr3_fnptr3_guard1_direct
#else
# error "Select STCXX_TARGET_MCS251 for the C++ runtime"
#endif

void STCXX_ABI_IDENTITY_SYMBOL(void);

/* Generated constructor bridge.  It must exist even when count is zero. */
void __stcxx_bridge_require_abi(void);
uint16_t __stcxx_bridge_ctor_count(void);
void __stcxx_bridge_invoke_ctor(uint16_t index);

void __stcxx_run_global_ctors(void);
stcxx_ctor_state_t stcxx_global_ctor_state(void);

void __cxa_pure_virtual(void);
void __cxa_deleted_virtual(void);

void stcxx_set_panic_hook(stcxx_panic_hook_t hook);
void stcxx_set_oom_hook(stcxx_oom_hook_t hook);
void stcxx_runtime_panic(stcxx_panic_reason_t reason);
void stcxx_out_of_memory(size_t requested_size);

#ifdef __cplusplus
}
#endif

#endif
