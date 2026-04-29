#include <vector>
#include <stdlib.h>
#include <stdio.h>

#define CL_TARGET_OPENCL_VERSION 120
#include <CL/opencl.h>

#include <tests/glsc2/debug.hpp>
#include <tests/glsc2/backend/common.h>
#include <constants.device.h>
#include <types.device.h>

#include "config.h"
#include "kernel.o.c"


#define RED_B_COLOR "\033[31;1m"
#define GREEN_B_COLOR "\033[32;1m"
#define WHITE_B_COLOR "\033[1m" 
#define RESET_T_COLOR "\033[0m"

#define TEST_INIT { printf(WHITE_B_COLOR "INFO" RESET_T_COLOR ": Running %s\n", __func__); }
#define TEST_PASS { printf(GREEN_B_COLOR "PASS" RESET_T_COLOR ": %s\n", __func__); return; }
#define TEST_FAIL { printf(RED_B_COLOR "FAIL" RESET_T_COLOR ": %s\n", __func__); return; }

size_t num_threads = DEVICE_SUB_GROUP_THREADS*TEST_NUMBER_SUB_GROUPS;

cl_context_wrapper wrapper;
cl_program program;

void test_local_1dim_broadcast();
void test_local_1dim_ballot();
void test_local_1dim_reduce_or();
void test_local_1dim_scan_inclusive_or();
void test_local_1dim_scan_inclusive_add();
void test_multiple_sync();

int main() {
    wrapper = cl_context_factory();

    size_t length = kernel_o_len;
    const unsigned char* binary = kernel_o;
    program = CL_CHECK2(clCreateProgramWithBinary(wrapper.context, 1, &wrapper.device_id, &length, &binary, NULL, &_err));
    CL_CHECK(clBuildProgram(program, 1, &wrapper.device_id, NULL, NULL, NULL));

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

    // order by dependencies
    test_local_1dim_broadcast();
    test_local_1dim_scan_inclusive_add();
    test_local_1dim_scan_inclusive_or();
    test_local_1dim_reduce_or();
    test_local_1dim_ballot();
    test_multiple_sync();
    
}

void test_local_1dim_ballot() {
    TEST_INIT;
    cl_kernel kernel = CL_CHECK2(clCreateKernel(program, __func__, &_err));

    // setup
    cl_uchar input[num_threads];
    sub_group_mask_t output[num_threads];
    for(int thread = 0; thread < num_threads; ++thread) 
        input[thread] = rand() % 2;
    cl_mem g_input = CL_CHECK2(clCreateBuffer(wrapper.context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, sizeof(input), &input, &_err));
    cl_mem g_output = CL_CHECK2(clCreateBuffer(wrapper.context, CL_MEM_WRITE_ONLY, sizeof(output), NULL, &_err));
    CL_CHECK(clSetKernelArg(kernel, 0, sizeof(g_input), &g_input));
    CL_CHECK(clSetKernelArg(kernel, 1, sizeof(g_output), &g_output));
    
    // run
    size_t lws[] = {DEVICE_SUB_GROUP_THREADS, TEST_NUMBER_SUB_GROUPS};
    cl_command_queue command_queue = clCreateCommandQueue(wrapper.context, wrapper.device_id, 0, NULL);
    CL_CHECK(clEnqueueNDRangeKernel(command_queue, kernel, 2, NULL, lws, lws, 0, NULL, NULL));
    CL_CHECK(clEnqueueReadBuffer(command_queue, g_output, CL_TRUE, 0, sizeof(output), output, 0, NULL, NULL));

    // check
    for(int subgroup = 0; subgroup < TEST_NUMBER_SUB_GROUPS; ++subgroup) {
        sub_group_mask_t *outptr    = &output[subgroup*DEVICE_SUB_GROUP_THREADS];
        cl_uchar         *inptr     = &input [subgroup*DEVICE_SUB_GROUP_THREADS]; 
        sub_group_mask_t tmp = outptr[0];

        for(int sg_id = 0; sg_id < DEVICE_SUB_GROUP_THREADS; ++sg_id) {
            // test if in subgroup share same value
            if (tmp.mask != outptr[sg_id].mask) {
                printf("Error mask[0]=%x != mask[%d]=%x\n", tmp.mask, sg_id, outptr[sg_id].mask);
                TEST_FAIL;
            }
            if (inptr[sg_id] != ((outptr[sg_id].mask >> sg_id) & 0x1u)) {
                printf("input [%d]=%d\n", sg_id, inptr [sg_id]);
                printf("output[%d]=%032b\n", sg_id, outptr[sg_id].mask);
                TEST_FAIL;
            }
        }
    }
    TEST_PASS;
}

void test_local_1dim_reduce_or() {
    TEST_INIT;
    cl_kernel kernel = CL_CHECK2(clCreateKernel(program, __func__, &_err));

    // setup
    cl_uint input[num_threads];
    cl_uint output[num_threads];
    for(int thread = 0; thread < num_threads; ++thread) 
        input[thread] = (1 << (thread%DEVICE_SUB_GROUP_THREADS));
        
    cl_mem g_input = CL_CHECK2(clCreateBuffer(wrapper.context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, sizeof(input), &input, &_err));
    cl_mem g_output = CL_CHECK2(clCreateBuffer(wrapper.context, CL_MEM_WRITE_ONLY, sizeof(output), NULL, &_err));
    CL_CHECK(clSetKernelArg(kernel, 0, sizeof(g_input), &g_input));
    CL_CHECK(clSetKernelArg(kernel, 1, sizeof(g_output), &g_output));
    
    // run
    size_t lws[] = {DEVICE_SUB_GROUP_THREADS, TEST_NUMBER_SUB_GROUPS};
    cl_command_queue command_queue = clCreateCommandQueue(wrapper.context, wrapper.device_id, 0, NULL);
    CL_CHECK(clEnqueueNDRangeKernel(command_queue, kernel, 2, NULL, lws, lws, 0, NULL, NULL));
    CL_CHECK(clEnqueueReadBuffer(command_queue, g_output, CL_TRUE, 0, sizeof(output), output, 0, NULL, NULL));

    // check
    for (int n=0; n<num_threads; ++n) {
        cl_uint expected = -1;
        if (output[n] != expected) {
            printf("ERROR: output[%d]=%08x, expected=%08x\n", n,  output[n], expected);
            TEST_FAIL;
        }
    }
    TEST_PASS;
}

void test_local_1dim_scan_inclusive_or() {
    TEST_INIT;
    cl_kernel kernel = CL_CHECK2(clCreateKernel(program, __func__, &_err));

    // setup
    cl_uint input[num_threads];
    cl_uint output[num_threads];
    for(int thread = 0; thread < num_threads; ++thread) 
        input[thread] = (1 << (thread%DEVICE_SUB_GROUP_THREADS));
        
    cl_mem g_input = CL_CHECK2(clCreateBuffer(wrapper.context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, sizeof(input), &input, &_err));
    cl_mem g_output = CL_CHECK2(clCreateBuffer(wrapper.context, CL_MEM_WRITE_ONLY, sizeof(output), NULL, &_err));
    CL_CHECK(clSetKernelArg(kernel, 0, sizeof(g_input), &g_input));
    CL_CHECK(clSetKernelArg(kernel, 1, sizeof(g_output), &g_output));
    
    // run
    size_t lws[] = {DEVICE_SUB_GROUP_THREADS, TEST_NUMBER_SUB_GROUPS};
    cl_command_queue command_queue = clCreateCommandQueue(wrapper.context, wrapper.device_id, 0, NULL);
    CL_CHECK(clEnqueueNDRangeKernel(command_queue, kernel, 2, NULL, lws, lws, 0, NULL, NULL));
    CL_CHECK(clEnqueueReadBuffer(command_queue, g_output, CL_TRUE, 0, sizeof(output), output, 0, NULL, NULL));

    // check
    for (int n=0; n<num_threads; ++n) {
        cl_uint expected = ((2 << n%DEVICE_SUB_GROUP_THREADS)-1);
        if (output[n] != expected) {
            printf("ERROR: output[%d]=%08x, expected=%08x\n", n, output[n], expected);
            TEST_FAIL;
        }
    }
    TEST_PASS;
}

void test_local_1dim_scan_inclusive_add() {
    TEST_INIT;
    cl_kernel kernel = CL_CHECK2(clCreateKernel(program, __func__, &_err));

    // setup
    cl_uint input[num_threads];
    cl_uint output[num_threads];
    for(int thread = 0; thread < num_threads; ++thread) 
        input[thread] = thread%DEVICE_SUB_GROUP_THREADS;
        
    cl_mem g_input = CL_CHECK2(clCreateBuffer(wrapper.context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, sizeof(input), &input, &_err));
    cl_mem g_output = CL_CHECK2(clCreateBuffer(wrapper.context, CL_MEM_WRITE_ONLY, sizeof(output), NULL, &_err));
    CL_CHECK(clSetKernelArg(kernel, 0, sizeof(g_input), &g_input));
    CL_CHECK(clSetKernelArg(kernel, 1, sizeof(g_output), &g_output));
    
    // run
    size_t lws[] = {DEVICE_SUB_GROUP_THREADS, TEST_NUMBER_SUB_GROUPS};
    cl_command_queue command_queue = clCreateCommandQueue(wrapper.context, wrapper.device_id, 0, NULL);
    CL_CHECK(clEnqueueNDRangeKernel(command_queue, kernel, 2, NULL, lws, lws, 0, NULL, NULL));
    CL_CHECK(clEnqueueReadBuffer(command_queue, g_output, CL_TRUE, 0, sizeof(output), output, 0, NULL, NULL));

    // check
    for (int n=0; n<num_threads; ++n) {
        uint thread = n%DEVICE_SUB_GROUP_THREADS;
        cl_uint expected = ((thread)*(thread+1))/2;
        if (output[n] != expected) {
            printf("ERROR: output[%d]=%d, expected=%d\n", n, output[n], expected);
            TEST_FAIL;
        }
    }
    TEST_PASS;
}

void test_local_1dim_broadcast() {
    TEST_INIT;
    cl_kernel kernel = CL_CHECK2(clCreateKernel(program, __func__, &_err));

    // setup
    cl_uint input[num_threads];
    cl_uint input_idx[num_threads];
    cl_uint output[num_threads];
    for(int thread = 0; thread < num_threads; ++thread) {
        input[thread] = rand();
        input_idx[thread] = rand() % DEVICE_SUB_GROUP_THREADS;
    }
        
    cl_mem g_input = CL_CHECK2(clCreateBuffer(wrapper.context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, sizeof(input), &input, &_err));
    cl_mem g_input_idx = CL_CHECK2(clCreateBuffer(wrapper.context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, sizeof(input_idx), &input_idx, &_err));
    cl_mem g_output = CL_CHECK2(clCreateBuffer(wrapper.context, CL_MEM_WRITE_ONLY, sizeof(output), NULL, &_err));
    CL_CHECK(clSetKernelArg(kernel, 0, sizeof(g_input), &g_input));
    CL_CHECK(clSetKernelArg(kernel, 1, sizeof(g_input_idx), &g_input_idx));
    CL_CHECK(clSetKernelArg(kernel, 2, sizeof(g_output), &g_output));
    
    // run
    size_t lws[] = {DEVICE_SUB_GROUP_THREADS, TEST_NUMBER_SUB_GROUPS};
    cl_command_queue command_queue = clCreateCommandQueue(wrapper.context, wrapper.device_id, 0, NULL);
    CL_CHECK(clEnqueueNDRangeKernel(command_queue, kernel, 2, NULL, lws, lws, 0, NULL, NULL));
    CL_CHECK(clEnqueueReadBuffer(command_queue, g_output, CL_TRUE, 0, sizeof(output), output, 0, NULL, NULL));

    // check
    for (int n=0; n<num_threads; ++n) {
        uint thread = n%DEVICE_SUB_GROUP_THREADS;

        cl_uint expected = input[(n-thread)+input_idx[n]];
        if (output[n] != expected) {
            printf("ERROR: output[%d]=%d, expected=%d\n", n, output[n], expected);
            TEST_FAIL;
        }
    }
    TEST_PASS;
}

void test_multiple_sync() {
    TEST_INIT;
    cl_kernel kernel = CL_CHECK2(clCreateKernel(program, __func__, &_err));

    // setup
    cl_uint input[num_threads];
    cl_uint input_idx[num_threads];
    cl_uint output[num_threads];
    for(int thread = 0; thread < num_threads; ++thread) {
        input[thread] = rand();
        input_idx[thread] = rand() % DEVICE_SUB_GROUP_THREADS;
    }
        
    cl_mem g_input = CL_CHECK2(clCreateBuffer(wrapper.context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, sizeof(input), &input, &_err));
    cl_mem g_input_idx = CL_CHECK2(clCreateBuffer(wrapper.context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, sizeof(input_idx), &input_idx, &_err));
    cl_mem g_output = CL_CHECK2(clCreateBuffer(wrapper.context, CL_MEM_WRITE_ONLY, sizeof(output), NULL, &_err));
    CL_CHECK(clSetKernelArg(kernel, 0, sizeof(g_input), &g_input));
    CL_CHECK(clSetKernelArg(kernel, 1, sizeof(g_input_idx), &g_input_idx));
    CL_CHECK(clSetKernelArg(kernel, 2, sizeof(g_output), &g_output));
    
    // run
    size_t lws[] = {DEVICE_SUB_GROUP_THREADS, TEST_NUMBER_SUB_GROUPS};
    cl_command_queue command_queue = clCreateCommandQueue(wrapper.context, wrapper.device_id, 0, NULL);
    CL_CHECK(clEnqueueNDRangeKernel(command_queue, kernel, 2, NULL, lws, lws, 0, NULL, NULL));
    
    if(clFinish(command_queue)) TEST_FAIL;

    TEST_PASS;
}