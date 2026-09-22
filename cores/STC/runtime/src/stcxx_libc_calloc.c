#if !defined(__SDCC_mcs251)
# error "The STC C++ native runtime requires SDCC MCS251"
#endif


#include <stddef.h>
#include <stdlib.h>
#include <string.h>


#if !defined(__SDCC_STACK_AUTO)
# error "the C++ libc ABI wrappers require SDCC --stack-auto"
#endif

/* MCS251 uses the same flat 24-bit representation for generic/XDATA pointers. */
typedef char stcxx_generic_pointer_must_be_24_bit[(sizeof(void *) == 3) ? 1 : -1];
typedef char stcxx_xdata_pointer_must_be_24_bit[(sizeof(void __xdata *) == 3) ? 1 : -1];

void *__stcxx_libc_calloc(size_t count, size_t size)
{
  void __xdata *pointer = calloc(count, size);
  return pointer;
}
