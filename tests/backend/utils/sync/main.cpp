#include <vector>
#include <algorithm>
#include <limits>
#include <stdlib.h>
#include <stdio.h>

#define CL_TARGET_OPENCL_VERSION 120
#include <CL/opencl.h>

#include <tests/backend/debug.hpp>
#include <tests/backend/common.h>
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

cl_uint* generate_random_uint_input() {
    cl_uint* input = (cl_uint*)malloc(sizeof(cl_uint) * num_threads);
    for(size_t i = 0; i < num_threads; ++i) {
        input[i] = rand() % 1000; // random uint
    }
    return input;
}

cl_uint* generate_random_1dim_idx_input() {
    cl_uint* input_idx = (cl_uint*)malloc(sizeof(cl_uint) * num_threads);
    for(size_t i = 0; i < num_threads; ++i) {
        input_idx[i] = rand() % DEVICE_SUB_GROUP_THREADS; // random index 0 to DEVICE_SUB_GROUP_THREADS-1
    }
    return input_idx;
}

void test_local_1dim_barrier();
void test_local_1dim_ballot();
void test_local_1dim_reduce_min();
void test_local_reduce_min();
void test_local_1dim_broadcast();
void test_local_1dim_scan_inclusive_add();
void test_local_scan_inclusive_add();
void test_local_1dim_scan_inclusive_add_bool();
void test_local_scan_inclusive_add_bool();
void test_multiple_sync();

template<typename T>
int compare_output(T* output, T* expected)
{
    for(int thread = 0; thread < num_threads; ++thread) {
        // test if in subgroup share same value
        if (output[thread] != expected[thread])
        {
            printf("Error at %d. output=%d != expected=%d\n", thread, (cl_uint) output[thread], (cl_uint) expected[thread]);
            return 1;

        }
    }

    return 0;
}

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
    test_local_1dim_barrier();
    test_local_1dim_ballot();
    test_local_1dim_reduce_min();
    test_local_reduce_min();
    test_local_1dim_broadcast();
    test_local_1dim_scan_inclusive_add();
    test_local_scan_inclusive_add();
    test_local_1dim_scan_inclusive_add_bool();
    test_local_scan_inclusive_add_bool();
    test_multiple_sync();
}

void test_local_1dim_barrier()
{
    TEST_INIT;
    cl_kernel kernel = CL_CHECK2(clCreateKernel(program, __func__, &_err));

    cl_uint *input = generate_random_uint_input();
    cl_uint *input_idx = generate_random_1dim_idx_input();
    cl_uint output[num_threads];
    cl_uint output_a[num_threads];

    cl_mem g_input = CL_CHECK2(clCreateBuffer(wrapper.context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, sizeof(cl_uint[num_threads]), input, &_err));
    CL_CHECK(clSetKernelArg(kernel, 0, sizeof(g_input), &g_input));
    cl_mem g_input_idx = CL_CHECK2(clCreateBuffer(wrapper.context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, sizeof(cl_uint[num_threads]), input_idx, &_err));
    CL_CHECK(clSetKernelArg(kernel, 1, sizeof(g_input_idx), &g_input_idx));
    cl_mem g_output = CL_CHECK2(clCreateBuffer(wrapper.context, CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR, sizeof(cl_uint[num_threads]), output, &_err));
    CL_CHECK(clSetKernelArg(kernel, 2, sizeof(g_output), &g_output));
    cl_mem g_output_a = CL_CHECK2(clCreateBuffer(wrapper.context, CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR, sizeof(cl_uint[num_threads]), output_a, &_err));
    CL_CHECK(clSetKernelArg(kernel, 3, sizeof(g_output_a), &g_output_a));

    size_t lws[] = {DEVICE_SUB_GROUP_THREADS, TEST_NUMBER_SUB_GROUPS};
    cl_command_queue command_queue = clCreateCommandQueue(wrapper.context, wrapper.device_id, 0, NULL);
    CL_CHECK(clEnqueueNDRangeKernel(command_queue, kernel, 2, NULL, lws, lws, 0, NULL, NULL));
    void *tmp_ptr;
    tmp_ptr = CL_CHECK2(clEnqueueMapBuffer(command_queue, g_output, CL_TRUE, CL_MAP_READ, 0, sizeof(cl_uint[num_threads]), 0, NULL, NULL, &_err));
    tmp_ptr = CL_CHECK2(clEnqueueMapBuffer(command_queue, g_output_a, CL_TRUE, CL_MAP_READ, 0, sizeof(cl_uint[num_threads]), 0, NULL, NULL, &_err));

    cl_uint expected[num_threads];
    cl_uint expected_a[num_threads];

    for(int thread = 0; thread < num_threads; ++thread)
        expected_a[thread] = 0;

    for(int thread = 0; thread < num_threads; ++thread)
        input_idx[thread] += (thread/DEVICE_SUB_GROUP_THREADS) * DEVICE_SUB_GROUP_THREADS;

    for(int thread = 0; thread < num_threads; ++thread)
    {
        expected[thread] = input[input_idx[thread]];
        expected_a[input_idx[thread]] += input[thread];
    }

    free(input);
    free(input_idx);
    CL_CHECK(clReleaseMemObject(g_input));
    CL_CHECK(clReleaseMemObject(g_input_idx));
    CL_CHECK(clReleaseMemObject(g_output));
    CL_CHECK(clReleaseMemObject(g_output_a));
    
    printf("\tCompare local memory sync.\n");
    if (compare_output(output, expected)) TEST_FAIL;
    printf("\tCompare local memory atomic sync.\n");
    if (compare_output(output_a, expected_a)) TEST_FAIL;

    TEST_PASS;
}

void test_local_1dim_ballot() {
    TEST_INIT;
    cl_kernel kernel = CL_CHECK2(clCreateKernel(program, __func__, &_err));

    // setup
    cl_uchar input[num_threads];
    for(int thread = 0; thread < num_threads; ++thread) input[thread] = rand() % 2;
    sub_group_mask_t output[num_threads];

    cl_mem g_input = CL_CHECK2(clCreateBuffer(wrapper.context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, sizeof(input), &input, &_err));
    CL_CHECK(clSetKernelArg(kernel, 0, sizeof(g_input), &g_input));
    cl_mem g_output = CL_CHECK2(clCreateBuffer(wrapper.context, CL_MEM_WRITE_ONLY, sizeof(output), NULL, &_err));
    CL_CHECK(clSetKernelArg(kernel, 1, sizeof(g_output), &g_output));
    
    // run
    size_t lws[] = {DEVICE_SUB_GROUP_THREADS, TEST_NUMBER_SUB_GROUPS};
    cl_command_queue command_queue = clCreateCommandQueue(wrapper.context, wrapper.device_id, 0, NULL);
    CL_CHECK(clEnqueueNDRangeKernel(command_queue, kernel, 2, NULL, lws, lws, 0, NULL, NULL));
    CL_CHECK(clEnqueueReadBuffer(command_queue, g_output, CL_TRUE, 0, sizeof(output), output, 0, NULL, NULL));


    sub_group_mask_t expected[num_threads];
    for(int thread = 0; thread < num_threads; ++thread) expected[thread].mask = 0;

    for(int thread = 0; thread < num_threads; ++thread)
    {
        int sub_group_offset = (thread/DEVICE_SUB_GROUP_THREADS) * DEVICE_SUB_GROUP_THREADS;
        for(int sg_thread = 0; sg_thread < DEVICE_SUB_GROUP_THREADS; ++sg_thread)
            expected[sub_group_offset + sg_thread].mask |= ((ulong) input[thread]) << (thread % DEVICE_SUB_GROUP_THREADS);
    }

    for(int thread = 0; thread < num_threads; ++thread) if (output[thread].mask != expected[thread].mask)
    {
        printf("Error %d. output=%lx != expected=%lx\n", thread, (ulong) output[thread].mask, (ulong) expected[thread].mask);
        TEST_FAIL;
    }

    /*
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
            // test if mask value is correct
            if (inptr[sg_id] != ((outptr[sg_id].mask >> sg_id) & 0x1u)) {
                printf("input [%d]=%d\n", sg_id, inptr [sg_id]);
                printf("output[%d]=%032b\n", sg_id, outptr[sg_id].mask);
                TEST_FAIL;
            }
        }
    }
    */
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

void test_local_1dim_reduce_min() {
    TEST_INIT;
    cl_kernel kernel = CL_CHECK2(clCreateKernel(program, __func__, &_err));

    // setup
    cl_uint input[num_threads];
    cl_uint output[num_threads];
    for(int thread = 0; thread < num_threads; ++thread) 
        input[thread] = rand() % 100; // small range for min
        
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
        cl_uint expected_min = CL_UINT_MAX;
        for(int sg_id = 0; sg_id < DEVICE_SUB_GROUP_THREADS; ++sg_id) {
            int idx = subgroup * DEVICE_SUB_GROUP_THREADS + sg_id;
            expected_min = std::min(expected_min, input[idx]);
        }
        for(int sg_id = 0; sg_id < DEVICE_SUB_GROUP_THREADS; ++sg_id) {
            int idx = subgroup * DEVICE_SUB_GROUP_THREADS + sg_id;
            if (output[idx] != expected_min) {
                printf("Error at %d. output=%u != expected_min=%u\n", idx, output[idx], expected_min);
                TEST_FAIL;
            }
        }
    }
    TEST_PASS;
}

void test_local_reduce_min() {
    TEST_INIT;
    cl_kernel kernel = CL_CHECK2(clCreateKernel(program, __func__, &_err));

    // setup
    cl_uint input[num_threads];
    cl_uint output[num_threads];
    for(int thread = 0; thread < num_threads; ++thread) 
        input[thread] = rand() % 100; // small range for min
        
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
    cl_uint expected_min = CL_UINT_MAX;
    for(int thread = 0; thread < num_threads; ++thread) 
    {
        expected_min = std::min(expected_min, input[thread]);
    }
    
    for(int thread = 0; thread < num_threads; ++thread) {
        if (output[thread] != expected_min) {
            printf("Error at %d. output=%u != expected_min=%u\n", thread, output[thread], expected_min);
            TEST_FAIL;
        }
    }
    TEST_PASS;
}

void test_local_scan_inclusive_add() {
    TEST_INIT;
    cl_kernel kernel = CL_CHECK2(clCreateKernel(program, __func__, &_err));

    // setup
    cl_uint input[num_threads];
    cl_uint output[num_threads];
    for(int thread = 0; thread < num_threads; ++thread) input[thread] = rand() % 1000;
        
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
    cl_uint expected[num_threads];
    for(int thread=0; thread < num_threads; ++thread)
    {
        expected[thread] = 0;

        for(int scan_thread=0; scan_thread <= thread; ++scan_thread)
        {
            expected[thread] += input[scan_thread];
        }
    }

    if (compare_output(output, expected)) TEST_FAIL;

    TEST_PASS;
}

void test_local_scan_inclusive_add_bool() {
    TEST_INIT;
    cl_kernel kernel = CL_CHECK2(clCreateKernel(program, __func__, &_err));

    // setup
    cl_uint input[num_threads];
    cl_uint output[num_threads];
    for(int thread = 0; thread < num_threads; ++thread) 
        input[thread] = (thread*991) % 2;
        
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
    int scan_value = 0;
    for (int n=0; n<num_threads; ++n) {
        scan_value += input[n];
        if (output[n] != scan_value) {
            printf("ERROR: output[%d]=%d, expected=%d\n", n, output[n], scan_value);
            TEST_FAIL;
        }
    }
    TEST_PASS;
}

void test_local_1dim_scan_inclusive_add_bool() {
    TEST_INIT;
    cl_kernel kernel = CL_CHECK2(clCreateKernel(program, __func__, &_err));

    // setup
    cl_uint input[num_threads];
    cl_uint output[num_threads];
    for(int thread = 0; thread < num_threads; ++thread) 
        input[thread] = (thread%27) & 1u;
        
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
    cl_uint expected;
    for (int n=0; n<num_threads; ++n) {
        if (n%DEVICE_SUB_GROUP_THREADS == 0) expected = 0;
        expected += input[n]; 
        if (output[n] != expected) {
            printf("ERROR: output[%d]=%08x, expected=%08x\n", n, output[n], expected);
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