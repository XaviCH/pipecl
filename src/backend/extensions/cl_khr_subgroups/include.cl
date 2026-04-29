#ifndef BACKEND_EXTENSIONS_CL_KHR_SUBGROUPS
#define BACKEND_EXTENSIONS_CL_KHR_SUBGROUPS

#include <config.device.h>

#ifdef cl_khr_subgroups
    #pragma OPENCL EXTENSION cl_khr_subgroups : enable
#else
    #ifdef __opencl_c_subgroups
        #pragma OPENCL EXTENSION __opencl_c_subgroups : enable
    #else
        #if (USE_CL_KHR_SUBGROUPS_IMPL == 1)
            #include <backend/extensions/cl_khr_subgroups/nv-sm_80+.cl>
        #endif
    #endif
#endif

#endif