#include <backend/utils/sync.cl>

#include "config.h"

kernel 
__attribute__((reqd_work_group_size(DEVICE_SUB_GROUP_THREADS, TEST_NUMBER_SUB_GROUPS, 1)))
void test_local_1dim_ballot(
    global uchar* g_input,
    global sub_group_mask_t* g_output
) {
    local sub_group_mask_t sg_temp[TEST_NUMBER_SUB_GROUPS][DEVICE_SUB_GROUP_THREADS];

    bool input = g_input[get_local_linear_id()];
    sub_group_mask_t result = local_1dim_ballot(input, &sg_temp[get_sub_group_id()]);
    g_output[get_local_linear_id()] = result;
}

kernel 
__attribute__((reqd_work_group_size(DEVICE_SUB_GROUP_THREADS, TEST_NUMBER_SUB_GROUPS, 1)))
void test_local_1dim_reduce_or(
    global uint* g_input,
    global uint* g_output
) {
    local uint sg_temp[TEST_NUMBER_SUB_GROUPS][DEVICE_SUB_GROUP_THREADS];

    uint input = g_input[get_local_linear_id()];
    uint result = local_1dim_reduce_or(input, &sg_temp[get_sub_group_id()]);
    g_output[get_local_linear_id()] = result;
}

kernel 
__attribute__((reqd_work_group_size(DEVICE_SUB_GROUP_THREADS, TEST_NUMBER_SUB_GROUPS, 1)))
void test_local_1dim_scan_inclusive_or(
    global uint* g_input,
    global uint* g_output
) {
    local uint sg_temp[TEST_NUMBER_SUB_GROUPS][DEVICE_SUB_GROUP_THREADS];

    uint input = g_input[get_local_linear_id()];
    uint result = local_1dim_scan_inclusive_or(input, &sg_temp[get_sub_group_id()]);
    g_output[get_local_linear_id()] = result;
}

kernel 
__attribute__((reqd_work_group_size(DEVICE_SUB_GROUP_THREADS, TEST_NUMBER_SUB_GROUPS, 1)))
void test_local_1dim_scan_inclusive_add(
    global uint* g_input,
    global uint* g_output
) {
    local uint sg_temp[TEST_NUMBER_SUB_GROUPS][DEVICE_SUB_GROUP_THREADS];

    uint input = g_input[get_local_linear_id()];
    uint result = local_1dim_scan_inclusive_add(input, &sg_temp[get_sub_group_id()]);
    g_output[get_local_linear_id()] = result;
}

kernel 
__attribute__((reqd_work_group_size(DEVICE_SUB_GROUP_THREADS, TEST_NUMBER_SUB_GROUPS, 1)))
void test_local_1dim_broadcast(
    global uint* g_input,
    global uint* g_input_idx,
    global uint* g_output
) {
    local uint sg_temp[TEST_NUMBER_SUB_GROUPS][DEVICE_SUB_GROUP_THREADS];

    uint input = g_input[get_local_linear_id()];
    uint input_idx = g_input_idx[get_local_linear_id()];
    uint result = local_1dim_broadcast(input, input_idx, &sg_temp[get_sub_group_id()]);
    g_output[get_local_linear_id()] = result;
}

kernel
__attribute__((reqd_work_group_size(DEVICE_SUB_GROUP_THREADS, TEST_NUMBER_SUB_GROUPS, 1)))
void test_multiple_sync(
    global uint* g_input,
    global uint* g_input_idx,
    global uint* g_output
) {
    local uint sg_temp[TEST_NUMBER_SUB_GROUPS][DEVICE_SUB_GROUP_THREADS];

    uint input = g_input[get_local_linear_id()];
    uint input_idx = g_input_idx[get_local_linear_id()];

    uint result = input;
    result = local_1dim_broadcast(result, input_idx, &sg_temp[get_sub_group_id()]);
    result = local_1dim_scan_inclusive_or(input, &sg_temp[get_sub_group_id()]);
    result = local_1dim_broadcast(result, input_idx, &sg_temp[get_sub_group_id()]);
    result = local_1dim_reduce_or(result, &sg_temp[get_sub_group_id()]);

    g_output[get_local_linear_id()] = result;
}