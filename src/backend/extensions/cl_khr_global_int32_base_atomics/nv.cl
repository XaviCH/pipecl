#ifndef BACKEND_EXTENSIONS_CL_KHR_GLOBAL_INT32_BASE_ATOMICS_NV_CL
#define BACKEND_EXTENSIONS_CL_KHR_GLOBAL_INT32_BASE_ATOMICS_NV_CL

#include <config.device.h>

#ifndef NVIDIA_SM_VERSION
    #error NVIDIA_SM_VERSION required to use this extension.
#endif

#if NVIDIA_SM_VERSION <= 0
    #error NVIDIA_SM_VERSION is not supported
#endif

// use to override global atomic_add implementation

int __attribute__((overloadable)) atomic_add(volatile global int *p, int val)
{
    int r;

    __asm__ volatile(
        "atom.global.add.s32  %0, [%1], %2;"
        : "=r"(r) : "l"(p), "r"(val));

    return r;

}

uint __attribute__((overloadable)) atomic_add(volatile global uint *p, uint val)
{
    uint r;

    __asm__ volatile(
        "atom.global.add.u32  %0, [%1], %2;"
        : "=r"(r) : "l"(p), "r"(val));

    return r;

}

#endif // BACKEND_EXTENSIONS_CL_KHR_GLOBAL_INT32_BASE_ATOMICS_NV_CL
