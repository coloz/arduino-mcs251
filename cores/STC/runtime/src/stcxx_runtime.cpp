
#include "stcxx_config.h"
#include "stcxx_runtime.h"

#if !STCXX_RUNTIME_HOST_TEST && !STCXX_TARGET_ABI
# error "Target runtime builds must enable the stc-arduino-cxx-v1 ABI checks"
#endif

static_assert(sizeof(stcxx_guard_t) == STCXX_GUARD_SIZE,
              "guard layout size does not match the runtime ABI");
static_assert(alignof(stcxx_guard_t) == 1u,
              "guard layout alignment does not match the runtime ABI");

static uint8_t stcxx_ctor_state_value;
static stcxx_panic_hook_t stcxx_panic_hook;
static stcxx_oom_hook_t stcxx_oom_hook;
static volatile uint8_t stcxx_panic_spin;

extern "C" void STCXX_ABI_IDENTITY_SYMBOL(void)
{
}

extern "C" void stcxx_set_panic_hook(stcxx_panic_hook_t hook)
{
    stcxx_panic_hook = hook;
}

extern "C" void stcxx_set_oom_hook(stcxx_oom_hook_t hook)
{
    stcxx_oom_hook = hook;
}

extern "C" void stcxx_runtime_panic(stcxx_panic_reason_t reason)
{
    if (stcxx_panic_hook != 0) {
        stcxx_panic_hook(reason);
    }

    /* A returning panic hook must never let execution continue unsafely. */
    for (;;) {
        stcxx_panic_spin = (uint8_t)(stcxx_panic_spin + 1u);
    }
}

extern "C" void stcxx_out_of_memory(size_t requested_size)
{
    if (stcxx_oom_hook != 0) {
        stcxx_oom_hook(requested_size);
    }

    /* The OOM hook is notification-only.  Throwing new must not return null. */
    stcxx_runtime_panic(STCXX_PANIC_OUT_OF_MEMORY);
}

extern "C" stcxx_ctor_state_t stcxx_global_ctor_state(void)
{
    return (stcxx_ctor_state_t)stcxx_ctor_state_value;
}

extern "C" void __stcxx_run_global_ctors(void)
{
    uint16_t count;
    uint16_t index;

    if (stcxx_ctor_state_value == (uint8_t)STCXX_CTORS_COMPLETE) {
        return;
    }
    if (stcxx_ctor_state_value != (uint8_t)STCXX_CTORS_NOT_STARTED) {
        stcxx_runtime_panic(STCXX_PANIC_CTOR_REENTRY);
    }

    /* This call creates a versioned link dependency before any ctor runs. */
    __stcxx_bridge_require_abi();
    stcxx_ctor_state_value = (uint8_t)STCXX_CTORS_RUNNING;
    count = __stcxx_bridge_ctor_count();
    for (index = 0u; index < count; ++index) {
        __stcxx_bridge_invoke_ctor(index);
    }
    stcxx_ctor_state_value = (uint8_t)STCXX_CTORS_COMPLETE;
}

extern "C" void __cxa_pure_virtual(void)
{
    stcxx_runtime_panic(STCXX_PANIC_PURE_VIRTUAL);
}

extern "C" void __cxa_deleted_virtual(void)
{
    stcxx_runtime_panic(STCXX_PANIC_DELETED_VIRTUAL);
}
