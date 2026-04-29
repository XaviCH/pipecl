#ifndef BACKEND_EXTENSIONS_CL_KHR_LOCAL_INT32_EXTENDED_ATOMICS_NV_CL
#define BACKEND_EXTENSIONS_CL_KHR_LOCAL_INT32_EXTENDED_ATOMICS_NV_CL

#include <config.device.h>

#ifndef NVIDIA_SM_VERSION
    #error NVIDIA_SM_VERSION required to use this extension.
#endif

#if NVIDIA_SM_VERSION <= 0
    #error NVIDIA_SM_VERSION is not supported
#endif

// use to override base atomic_or implementation

uint __attribute__((overloadable)) atomic_or(volatile local uint *p, uint val)
{
    uint r;

    __asm__ volatile(
        "atom.shared.or.b32  %0, [%1], %2;"
        : "=r"(r) : "rd"(p), "r"(val));

    return r;

}

#endif // BACKEND_EXTENSIONS_CL_KHR_LOCAL_INT32_EXTENDED_ATOMICS_NV_CL
