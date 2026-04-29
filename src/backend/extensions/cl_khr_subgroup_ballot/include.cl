#ifndef BACKEND_EXTENSIONS_CL_KHR_SUBGROUP_BALLOT
#define BACKEND_EXTENSIONS_CL_KHR_SUBGROUP_BALLOT

#include <config.device.h>

#ifdef cl_khr_subgroup_ballot
    #pragma OPENCL EXTENSION cl_khr_subgroup_ballot : enable
#else
    #if (USE_CL_KHR_SUBGROUP_BALLOT_IMPL == 1)
        #include <backend/extensions/cl_khr_subgroup_ballot/nv-sm_30+.cl>
    #endif
#endif

#endif