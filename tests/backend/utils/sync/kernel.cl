#include <backend/utils/sync.cl>

#include "config.h"


void local_1dim_unsync(local volatile uint l_temp[DEVICE_SUB_GROUP_THREADS*TEST_NUMBER_SUB_GROUPS])
{
    if (get_local_linear_id() % 2)
    {
        l_temp[get_local_id(0) * get_local_id(1)] = 0xffffu;
    }

    if (get_local_linear_id() % 3)
    {
        l_temp[get_local_id(1)] = 0xffff0000u;
    }

    if (get_local_linear_id() % 5)
    {
        l_temp[min(0u, (uint)(get_local_id(0) - get_local_id(1)))] = 0xffff0000u;
    }
}

// done
kernel 
__attribute__((reqd_work_group_size(DEVICE_SUB_GROUP_THREADS, TEST_NUMBER_SUB_GROUPS, 1)))
void test_local_1dim_barrier(
    global uint* g_input,
    global uint* g_input_idx,
    global uint* g_output_lmem,
    global uint* g_output_almem
) {
    local volatile uint l_temp[TEST_NUMBER_SUB_GROUPS*DEVICE_SUB_GROUP_THREADS];
    local volatile uint al_temp[TEST_NUMBER_SUB_GROUPS*DEVICE_SUB_GROUP_THREADS];
    al_temp[get_local_linear_id()] = 0;

    local_1dim_barrier(CLK_LOCAL_MEM_FENCE);

    uint input = g_input[get_local_linear_id()];
    uint input_idx = get_local_id(1)*DEVICE_SUB_GROUP_THREADS + g_input_idx[get_local_linear_id()];

    l_temp[get_local_linear_id()] = input;
    atomic_add(&al_temp[input_idx], input);

    local_1dim_barrier(CLK_LOCAL_MEM_FENCE);

    g_output_lmem[get_local_linear_id()] = l_temp[input_idx];
    g_output_almem[get_local_linear_id()] = al_temp[get_local_linear_id()];
}

// done
kernel 
__attribute__((reqd_work_group_size(DEVICE_SUB_GROUP_THREADS, TEST_NUMBER_SUB_GROUPS, 1)))
void test_local_1dim_ballot(
    global uchar* g_input,
    global sub_group_mask_t* g_output
) {
    local volatile sub_group_mask_t l_temp[TEST_NUMBER_SUB_GROUPS*DEVICE_SUB_GROUP_THREADS];
    local volatile uint l_unsync[TEST_NUMBER_SUB_GROUPS*DEVICE_SUB_GROUP_THREADS];

    bool input = g_input[get_local_linear_id()];

    local_1dim_unsync(l_unsync);

    sub_group_mask_t result = local_1dim_ballot(input, l_temp);
    
    g_output[get_local_linear_id()] = result;
}

// done
kernel 
__attribute__((reqd_work_group_size(DEVICE_SUB_GROUP_THREADS, TEST_NUMBER_SUB_GROUPS, 1)))
void test_local_1dim_reduce_min(
    global uint* g_input,
    global uint* g_output
) {
    local volatile uint l_temp[TEST_NUMBER_SUB_GROUPS*DEVICE_SUB_GROUP_THREADS];
    local volatile uint l_unsync[TEST_NUMBER_SUB_GROUPS*DEVICE_SUB_GROUP_THREADS];

    uint input = g_input[get_local_linear_id()];

    local_1dim_unsync(l_unsync);

    uint result = local_1dim_reduce_min(input, l_temp);

    g_output[get_local_linear_id()] = result;
}

// done
kernel 
__attribute__((reqd_work_group_size(DEVICE_SUB_GROUP_THREADS, TEST_NUMBER_SUB_GROUPS, 1)))
void test_local_reduce_min(
    global uint* g_input,
    global uint* g_output
) {
    local volatile uint l_temp[TEST_NUMBER_SUB_GROUPS*DEVICE_SUB_GROUP_THREADS];
    local volatile uint l_unsync[TEST_NUMBER_SUB_GROUPS*DEVICE_SUB_GROUP_THREADS];

    uint input = g_input[get_local_linear_id()];

    local_1dim_unsync(l_unsync);

    uint result = local_reduce_min(input, l_temp);

    g_output[get_local_linear_id()] = result;
}

// done
kernel 
__attribute__((reqd_work_group_size(DEVICE_SUB_GROUP_THREADS, TEST_NUMBER_SUB_GROUPS, 1)))
void test_local_1dim_broadcast(
    global uint* g_input,
    global uint* g_input_idx,
    global uint* g_output
) {
    local volatile uint l_temp[TEST_NUMBER_SUB_GROUPS*DEVICE_SUB_GROUP_THREADS];
    local volatile uint l_unsync[TEST_NUMBER_SUB_GROUPS*DEVICE_SUB_GROUP_THREADS];

    uint input = g_input[get_local_linear_id()];
    uint input_idx = g_input_idx[get_local_linear_id()];

    local_1dim_unsync(l_unsync);

    uint result = local_1dim_broadcast(input, input_idx, l_temp);
    
    g_output[get_local_linear_id()] = result;
}

// done
kernel 
__attribute__((reqd_work_group_size(DEVICE_SUB_GROUP_THREADS, TEST_NUMBER_SUB_GROUPS, 1)))
void test_local_1dim_scan_inclusive_add(
    global uint* g_input,
    global uint* g_output
) {
    local volatile uint l_temp[TEST_NUMBER_SUB_GROUPS*DEVICE_SUB_GROUP_THREADS];
    local volatile uint l_unsync[TEST_NUMBER_SUB_GROUPS*DEVICE_SUB_GROUP_THREADS];


    uint input = g_input[get_local_linear_id()];

    local_1dim_unsync(l_unsync);

    uint result = local_1dim_scan_inclusive_add(input, l_temp);

    g_output[get_local_linear_id()] = result;
}

// done
kernel 
__attribute__((reqd_work_group_size(DEVICE_SUB_GROUP_THREADS, TEST_NUMBER_SUB_GROUPS, 1)))
void test_local_scan_inclusive_add(
    global uint* g_input,
    global uint* g_output
) {
    local volatile uint l_temp[TEST_NUMBER_SUB_GROUPS*DEVICE_SUB_GROUP_THREADS];
    local volatile uint l_unsync[TEST_NUMBER_SUB_GROUPS*DEVICE_SUB_GROUP_THREADS];

    uint input = g_input[get_local_linear_id()];

    local_1dim_unsync(l_unsync);

    uint result = local_scan_inclusive_add(input, l_temp);

    g_output[get_local_linear_id()] = result;
}

// done
kernel 
__attribute__((reqd_work_group_size(DEVICE_SUB_GROUP_THREADS, TEST_NUMBER_SUB_GROUPS, 1)))
void test_local_1dim_scan_inclusive_add_bool(
    global uint* g_input,
    global uint* g_output
) {
    local volatile uint l_temp[TEST_NUMBER_SUB_GROUPS*DEVICE_SUB_GROUP_THREADS];
    local volatile uint l_unsync[TEST_NUMBER_SUB_GROUPS*DEVICE_SUB_GROUP_THREADS];

    uint input = g_input[get_local_linear_id()];

    local_1dim_unsync(l_unsync);

    uint result = local_1dim_scan_inclusive_add_bool(input, l_temp);

    g_output[get_local_linear_id()] = result;
}

// done
kernel 
__attribute__((reqd_work_group_size(DEVICE_SUB_GROUP_THREADS, TEST_NUMBER_SUB_GROUPS, 1)))
void test_local_scan_inclusive_add_bool(
    global uint* g_input,
    global uint* g_output
) {
    local volatile uint l_temp[TEST_NUMBER_SUB_GROUPS*DEVICE_SUB_GROUP_THREADS];
    local volatile uint l_unsync[TEST_NUMBER_SUB_GROUPS*DEVICE_SUB_GROUP_THREADS];

    uint input = g_input[get_local_linear_id()];

    local_1dim_unsync(l_unsync);
    
    uint result = local_scan_inclusive_add_bool(input ? 1 : 0, l_temp);

    g_output[get_local_linear_id()] = result;
}

// done
kernel
__attribute__((reqd_work_group_size(DEVICE_SUB_GROUP_THREADS, TEST_NUMBER_SUB_GROUPS, 1)))
void test_multiple_sync(
    global uint* g_input,
    global uint* g_input_idx,
    global uint* g_output
) {
    typedef union {
        uint integer[TEST_NUMBER_SUB_GROUPS*DEVICE_SUB_GROUP_THREADS];
        sub_group_mask_t mask[TEST_NUMBER_SUB_GROUPS*DEVICE_SUB_GROUP_THREADS];
    } l_temp_t;

    local volatile l_temp_t l_temp;
    local volatile uint l_unsync[TEST_NUMBER_SUB_GROUPS*DEVICE_SUB_GROUP_THREADS];

    uint input = g_input[get_local_linear_id()];
    uint input_idx = g_input_idx[get_local_linear_id()];

    local_1dim_unsync(l_unsync);

    uint result_1dim = local_1dim_reduce_min(input, l_temp.integer);
    uint result = local_reduce_min(input, l_temp.integer);

    bool me_1dim = result_1dim == input;
    bool me = result == input;

    sub_group_mask_t mask = local_1dim_ballot(me_1dim, l_temp.mask);
    uint number = (uint) mask.mask;

    if (!me) number += result;

    number = local_1dim_scan_inclusive_add(number, l_temp.integer);
    number = local_scan_inclusive_add(number, l_temp.integer);

    number = local_1dim_scan_inclusive_add_bool(number % 2 == 0 || number % 3 == 0, l_temp.integer);
    number = local_scan_inclusive_add_bool(number % 2 == 0 || number % 3 == 0, l_temp.integer);

    result = local_1dim_broadcast(number, input_idx, &l_temp.integer);

    g_output[get_local_linear_id()] = result;
}