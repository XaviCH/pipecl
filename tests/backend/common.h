#define CL_TARGET_OPENCL_VERSION 120

#include <CL/opencl.h>
#include "debug.hpp"
#include <config.device.h>

typedef struct {
    cl_context context;
    cl_device_id device_id;
    cl_platform_id platform_id;
} cl_context_wrapper;

cl_context_wrapper cl_context_factory() {
    cl_context_wrapper wrapper;
    
    CL_CHECK(clGetPlatformIDs(1, &wrapper.platform_id, NULL));

    // Choose device
    const cl_uint num_device_entries = DEVICE_DEVICE_ID + 1;
    
    cl_device_id device_entry_ids[num_device_entries];
    CL_CHECK(clGetDeviceIDs(wrapper.platform_id, CL_DEVICE_TYPE_ALL, num_device_entries, device_entry_ids, NULL));

    wrapper.device_id = device_entry_ids[DEVICE_DEVICE_ID];

    wrapper.context = CL_CHECK2(clCreateContext(NULL, 1, &wrapper.device_id, NULL, NULL, &_err));

    return wrapper;
}

cl_program cl_create_program_from_source(cl_context_wrapper wrapper, const char* source_code) {
    cl_program program = CL_CHECK2(clCreateProgramWithSource(wrapper.context, 1, &source_code, NULL, &_err));
    const char options[] = "-D__COMPILER_RELATIVE_PATH__";
    CL_CHECK(clBuildProgram(program, 1, &wrapper.device_id, options, NULL, NULL));

    cl_build_status status;
    CL_CHECK(clGetProgramBuildInfo(program, wrapper.device_id, CL_PROGRAM_BUILD_STATUS, sizeof(status), &status, NULL));
    if (status) {
        printf("ERROR: Program status = %d.\n", status);

        size_t size;
        CL_CHECK(clGetProgramBuildInfo(program, wrapper.device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &size));
        
        char* log = (char*)malloc(size);
        CL_CHECK(clGetProgramBuildInfo(program, wrapper.device_id, CL_PROGRAM_BUILD_LOG, size, log, NULL));

        printf("ERROR: %s\n", log);
        exit(1);
    }

    return program;
}