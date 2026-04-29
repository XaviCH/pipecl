#ifndef BACKEND_UTILS_SYNC_CL
#define BACKEND_UTILS_SYNC_CL

#include <backend/extensions/cl_khr_subgroups/include.cl>
#include <backend/extensions/cl_khr_subgroup_ballot/include.cl>
#include <backend/utils/common.cl>
#include <backend/utils/sub_group_mask.cl>

// --------------
// UTILS
// --------------

static inline uint __attribute__((overloadable)) add(uint a, uint b) { return a + b; }

/**
 * @brief Barrier syncronization for 1st dimension threads on work-group.
 */
static inline void local_1dim_barrier(cl_mem_fence_flags flags) 
{
    #ifdef DEVICE_SUB_GROUP_LOCKSTEP_RAW_ENABLED
        return;
    #endif

    #ifdef DEVICE_SUB_GROUP_INTRINSICTS_ENABLED
        sub_group_barrier(flags);
    #else
        barrier(flags);
    #endif
}

//------------------------------------------------------------

#ifdef DEVICE_SUB_GROUP_INTRINSICTS_ENABLED
    #define GEN_LOCAL_1DIM_BROADCAST_IMPL(_TYPE) \
        static inline _TYPE __attribute__((overloadable)) local_1dim_broadcast(_TYPE value, uint id, local volatile _TYPE* l_temp) \
        { \
            return sub_group_broadcast(value, id); \
        }
#else
    #define GEN_LOCAL_1DIM_BROADCAST_IMPL(_TYPE) \
        static inline _TYPE __attribute__((overloadable)) local_1dim_broadcast(_TYPE value, uint id, local volatile _TYPE* l_temp) \
        { \
            local volatile _TYPE *ptr = &l_temp[get_local_id(1)*get_local_size(0)]; \
            ptr[get_local_id(0)] = value; \
            local_1dim_barrier(CLK_LOCAL_MEM_FENCE); \
            uint result = ptr[id]; \
            barrier(CLK_LOCAL_MEM_FENCE); \
            return result; \
        }
#endif

GEN_LOCAL_1DIM_BROADCAST_IMPL(uint)

// --------------
// SCAN
// --------------

#ifdef DEVICE_SUB_GROUP_INTRINSICTS_ENABLED
    #define GEN_LOCAL_1DIM_SCAN_INCLUSIVE_IMPL(_FUNC, _TYPE) \
        static inline uint __attribute__((overloadable)) local_1dim_scan_inclusive_##_FUNC(_TYPE value, local volatile _TYPE* l_temp) \
        { \
            return sub_group_scan_inclusive_##_FUNC(value); \
        }
#else
    #define GEN_LOCAL_1DIM_SCAN_INCLUSIVE_IMPL(_FUNC, _TYPE) \
        static inline uint __attribute__((overloadable)) local_1dim_scan_inclusive_##_FUNC(_TYPE value, local volatile _TYPE* l_temp) \
        { \
            uint local_id = get_local_id(0); \
            local volatile _TYPE* ptr = &l_temp[get_local_linear_id()]; \
            *ptr = value; \
            for(int i=1; i<get_local_size(0); i=i*2) { \
                local_1dim_barrier(CLK_LOCAL_MEM_FENCE); \
                if (local_id >= i) { \
                    value = _FUNC(value, ptr[-i]); \
                    *ptr = value; \
                } \
            } \
            barrier(CLK_LOCAL_MEM_FENCE); \
            return value; \
        }
#endif

GEN_LOCAL_1DIM_SCAN_INCLUSIVE_IMPL(min, uint)
GEN_LOCAL_1DIM_SCAN_INCLUSIVE_IMPL(max, uint)
GEN_LOCAL_1DIM_SCAN_INCLUSIVE_IMPL(add, uint)

static inline uint __attribute__((overloadable)) local_1dim_scan_inclusive_add_bool(bool value, local volatile uint* l_temp)
{
    uint result;

    #ifdef DEVICE_SUB_GROUP_INTRINSICTS_ENABLED
    {
        sub_group_barrier(CLK_LOCAL_MEM_FENCE); // ballot does not guarantee that all threads are active, so we need to make sure that all threads have reached this point before calling ballot
        sub_group_mask_t mask = ballot_sub_group_mask(value);
        sub_group_mask_t scan = and_sub_group_mask(mask, get_lane_sub_group_mask_le());
        result = popcount_sub_group_mask(scan);
    }
    #else
    {
        result = local_1dim_scan_inclusive_add(value ? 1u : 0u, l_temp);
        barrier(CLK_LOCAL_MEM_FENCE); // make sure that all threads have the correct result before any thread can read it
    }
    #endif

    return result;
}

static inline uint __attribute__((overloadable)) local_scan_inclusive_add_bool(bool value, local volatile uint* l_temp)
{
    uint result_1dim = local_1dim_scan_inclusive_add_bool(value, l_temp);
    
    l_temp[get_local_linear_id()] = result_1dim; // ensure result;

    barrier(CLK_LOCAL_MEM_FENCE);

    uint accumulated = 0;
    
    if (get_local_id(0) < get_local_size(1)) 
    {
        accumulated = l_temp[(get_local_id(0)+1)*DEVICE_SUB_GROUP_THREADS - 1];
    }
    
    barrier(CLK_LOCAL_MEM_FENCE);

    accumulated = local_1dim_scan_inclusive_add(accumulated, l_temp) - accumulated; // scan exclusive

    uint result_2dim = local_1dim_broadcast(accumulated, get_local_id(1), l_temp);

    return result_2dim + result_1dim;
}

static inline uint __attribute__((overloadable)) local_scan_inclusive_add(uint value, local volatile uint* l_temp)
{
    uint result_1dim = local_1dim_scan_inclusive_add(value, l_temp);
    
    l_temp[get_local_linear_id()] = result_1dim; // ensure result;

    barrier(CLK_LOCAL_MEM_FENCE);

    uint accumulated = 0;
    
    if (get_local_id(0) < get_local_size(1)) 
    {
        accumulated = l_temp[(get_local_id(0)+1)*DEVICE_SUB_GROUP_THREADS - 1];
    }
    
    barrier(CLK_LOCAL_MEM_FENCE);

    accumulated = local_1dim_scan_inclusive_add(accumulated, l_temp) - accumulated; // scan exclusive

    uint result_2dim = local_1dim_broadcast(accumulated, get_local_id(1), l_temp);

    return result_2dim + result_1dim;
}

static inline uint __attribute__((overloadable)) local_scan_inclusive_min(uint value, local volatile uint* l_temp)
{
    uint result_1dim = local_1dim_scan_inclusive_min(value, l_temp);
    
    l_temp[get_local_linear_id()] = result_1dim; // ensure result;

    barrier(CLK_LOCAL_MEM_FENCE);

    uint value_2dim = UINT_MAX;
    
    if (get_local_id(0) < get_local_size(1)) 
    {
        value_2dim = l_temp[(get_local_id(0)+1)*DEVICE_SUB_GROUP_THREADS - 1];
    }
    
    barrier(CLK_LOCAL_MEM_FENCE);

    value_2dim = local_1dim_scan_inclusive_min(value_2dim, l_temp);

    uint result_2dim = local_1dim_broadcast(value_2dim, get_local_id(1), l_temp);

    return min(result_2dim, result_1dim);
}

// --------------
// REDUCE
// --------------

#ifdef DEVICE_SUB_GROUP_INTRINSICTS_ENABLED
    #define GEN_LOCAL_1DIM_REDUCE_IMPL(_FUNC, _TYPE) \
        static inline _TYPE __attribute__((overloadable)) local_1dim_reduce_##_FUNC(_TYPE value, local volatile _TYPE* l_temp) \
        { \
            return sub_group_reduce_##_FUNC(value); \
        }
#else
    #define GEN_LOCAL_1DIM_REDUCE_IMPL(_FUNC, _TYPE) \
        static inline _TYPE __attribute__((overloadable)) local_1dim_reduce_##_FUNC(_TYPE value, local volatile _TYPE* l_temp) \
        { \
            uint result = local_1dim_scan_inclusive_##_FUNC(value, l_temp); \
            return local_1dim_broadcast(result, get_local_size(0) - 1, l_temp); \
        }
#endif

GEN_LOCAL_1DIM_REDUCE_IMPL(min, uint)
GEN_LOCAL_1DIM_REDUCE_IMPL(max, uint)

static inline uint __attribute__((overloadable)) local_reduce_min(uint value, local volatile uint* l_temp) 
{
    uint result_1dim = local_1dim_reduce_min(value, l_temp);

    l_temp[get_local_id(1)] = result_1dim;

    barrier(CLK_LOCAL_MEM_FENCE); // ensure write

    uint tmp = UINT_MAX;
    if (get_local_id(0) < get_local_size(1)) 
        tmp = l_temp[get_local_id(0)];

    barrier(CLK_LOCAL_MEM_FENCE); // ensure read

    return local_1dim_reduce_min(tmp, l_temp);
}

// -------------
// BALLOT
// -------------

#ifdef DEVICE_SUB_GROUP_ENABLED
    static inline sub_group_mask_t __attribute__((overloadable)) local_1dim_ballot(bool value, local volatile sub_group_mask_t* l_temp) 
    {
        sub_group_mask_t mask;

        #ifdef DEVICE_SUB_GROUP_INTRINSICTS_ENABLED
        {
            sub_group_barrier(CLK_LOCAL_MEM_FENCE); // ensure sync ballot
            mask = ballot_sub_group_mask(value);
        }
        #else
        {
            sub_group_mask_t tmp;
            clear_sub_group_mask(&tmp);
            if (value) set_bit_sub_group_mask(&tmp, get_local_id(0));

            clear_sub_group_mask(&l_temp[get_local_id(1)]);

            local_1dim_barrier(CLK_LOCAL_MEM_FENCE);

            atomic_or_sub_group_mask(&l_temp[get_local_id(1)], tmp);
            
            local_1dim_barrier(CLK_LOCAL_MEM_FENCE);
            
            #ifdef DEVICE_BARRIER_SYNC_LOCAL_ATOMIC
            {
                mask = l_temp[get_local_id(1)];
            }
            #else
            {
                mask = atomic_or_sub_group_mask(&l_temp[get_local_id(1)], get_sub_group_mask_zero());
            }
            #endif
            
            barrier(CLK_LOCAL_MEM_FENCE);
        }
        #endif
        
        return mask;
    }
#endif

#endif // BACKEND_UTILS_SYNC_CL