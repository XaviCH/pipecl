#ifdef __COMPILER_RELATIVE_PATH__
#include <backend/types.cl>
#include <backend/utils/sub_group_mask.cl>
#include <backend/utils/sync.cl>
#else
#include "glsc2/src/backend/types.cl"
#include "glsc2/src/backend/utils/sub_group_mask.cl"
#include "glsc2/src/backend/utils/sync.cl"
#endif

#define FW_ARRAY_SIZE(X)    ((int)(sizeof(X) / sizeof((X)[0])))

// TODO: move this to config file
#define TRIANGLE_BUFFER_ELEMS (DEVICE_BIN_SUB_GROUPS*DEVICE_SUB_GROUP_THREADS*4)

static inline void local_fill_triangle_buffer(
    global const uchar* restrict g_tri_subtris,
    local volatile int* restrict s_tri_buf,
    local volatile uint* restrict s_batch_pos,
    local volatile uint* restrict s_buf_count,
    local volatile uint* restrict l_temp,
    private int* restrict batch_pos,
    private int* restrict buf_count,
    private int* restrict buf_index,
    const int batch_end
) {
    while (*buf_count < get_local_linear_size() && *batch_pos < batch_end) {
    
        // get subtriangle count
        int tri_idx = *batch_pos + get_local_linear_id();
        int num = 0;
        if (tri_idx < batch_end)
            num = g_tri_subtris[tri_idx];

        // cumulative sum of subtriangles within each subgroup
        uint scan_exc_num = local_scan_inclusive_add(num, l_temp) - num;

        if (get_local_linear_id() == get_local_linear_size()-1)
        {
            *s_batch_pos = *batch_pos + get_local_linear_size();
            *s_buf_count = *buf_count + scan_exc_num + num;
        }
        
        barrier(CLK_LOCAL_MEM_FENCE);

        // skip if no subtriangles
        if (num) {
            uint pos = *buf_count + scan_exc_num;

            // only write if entire triangle fits
            if (pos + num <= TRIANGLE_BUFFER_ELEMS)
            {
                pos = (pos + *buf_index) % TRIANGLE_BUFFER_ELEMS; // adjust for current start position
                if (num == 1)
                    s_tri_buf[pos] = (tri_idx << 3) | 0x7u; // single triangle
                else {
                    for (int i=0; i < num; i++) {
                        s_tri_buf[pos] = (tri_idx << 3) | i;
                        pos = (pos + 1) % TRIANGLE_BUFFER_ELEMS;
                    }
                }
            } else if (pos <= TRIANGLE_BUFFER_ELEMS)
            {
                // this triangle is the first that failed, overwrite total count and triangle count
                *s_batch_pos = *batch_pos + get_local_linear_id();
                *s_buf_count = pos;
            }
        }

        barrier(CLK_LOCAL_MEM_FENCE);
        *batch_pos = *s_batch_pos;
        *buf_count = *s_buf_count;
    }
}

/**
    Processed triangles are going to be stored in bins, depending if they fall inside.
    Each CTA has their full set of bins. Each CTA would batch a different set of triangles
    to test each bin. 
 */
kernel
#ifdef DEVICE_SUB_GROUP_ENABLED
__attribute__((reqd_work_group_size(DEVICE_SUB_GROUP_THREADS, DEVICE_BIN_SUB_GROUPS, 1)))
#else
    #error Kernel require sub groups to work.
#endif
void bin_raster(
    global int* a_bin_counter,
    global int* a_num_bin_segs,
    global const int* a_num_subtris,

    global int* g_bin_first_seg,
    global int* g_bin_seg_count,
    global int* g_bin_seg_data,
    global int* g_bin_seg_next,
    global int* g_bin_total,
    global const triangle_header_t* g_tri_header,
    global const uchar* g_tri_subtris,
    
    #ifdef DEVICE_IMAGE_ENABLED
    read_only image1d_buffer_t t_tri_header,
    #endif

    private const int c_bin_batch_sz,
    private const int c_height_bins,
    private const int c_max_bin_segs,
    private const int c_max_subtris,
    private const int c_num_bins,
    private const int c_num_tris,
    private const int c_viewport_height,
    private const int c_viewport_width,
    private const int c_width_bins
) {
    // Local space
    local volatile int  s_out_ofs     [CR_MAXBINS_SQR];
    local volatile int  s_out_total   [CR_MAXBINS_SQR];
    local volatile int  s_over_index  [CR_MAXBINS_SQR];

    local volatile sub_group_mask_t     s_out_mask    [DEVICE_BIN_SUB_GROUPS][CR_MAXBINS_SQR + 1];          // +1 to avoid bank collisions
    local volatile int                  s_out_count   [DEVICE_BIN_SUB_GROUPS][CR_MAXBINS_SQR + 1];          // +1 to avoid bank collisions
    local volatile int                  s_tri_buf     [TRIANGLE_BUFFER_ELEMS];   // triangle ring buffer

    local volatile uint s_batch_pos;
    local volatile uint s_buf_count;
    local volatile uint s_over_total;
    local volatile uint s_alloc_base;

    // #ifdef DEVICE_SUB_GROUP_INTRINSICTS_ENABLED
    // local volatile uint l_temp [DEVICE_BIN_SUB_GROUPS];
    // #else 
    local volatile uint l_temp [DEVICE_SUB_GROUP_THREADS*DEVICE_BIN_SUB_GROUPS];
    // #endif

    if (*a_num_subtris > c_max_subtris) 
        return;

    // Private space
    int local_id = get_local_linear_id();
    uint local_size = get_local_linear_size();
    int batch_pos = 0;

    // initialize output linked lists and offsets
    if (local_id < c_num_bins)
    {
        g_bin_first_seg[(local_id * CR_BIN_STREAMS_SIZE) + get_group_id(0)] = -1;
        s_out_ofs[local_id] = -CR_BIN_SEG_SIZE;
        s_out_total[local_id] = 0;
    }

    // repeat until done
    for (;;) {
        // get batch
        if (local_id == 0)
            s_batch_pos = atomic_add(a_bin_counter,c_bin_batch_sz);
        barrier(CLK_LOCAL_MEM_FENCE);
        batch_pos = s_batch_pos;

        // all batches done?
        if (batch_pos >= c_num_tris)
            break;

        // Private space
        int buf_index = 0;
        int buf_count = 0;
        int batch_end = min(batch_pos + c_bin_batch_sz, c_num_tris);

        // loop over batch
        do {
            
            // fill the s_tri_buf with tri_idx[31:3] + sub_tri_idx[2:0]
            local_fill_triangle_buffer(
                g_tri_subtris,
                s_tri_buf,
                &s_batch_pos,
                &s_buf_count,
                l_temp,
                &batch_pos,
                &buf_count,
                &buf_index,
                batch_end
            );

            // choose our triangle
            uint4 tri_data = (uint4){0, 0, 0, 0};
            if (local_id < buf_count)
            {
                uint tri_pos = buf_index + local_id;
                tri_pos %= TRIANGLE_BUFFER_ELEMS;

                // find triangle
                int tri_idx = s_tri_buf[tri_pos];
                int data_idx = tri_idx >> 3;
                int subtri_idx = tri_idx & 7;
                if (subtri_idx != 7)
                    data_idx = g_tri_header[data_idx].misc.misc + subtri_idx;

                // read triangle
                #ifdef DEVICE_IMAGE_ENABLED
                tri_data = read_imageui(t_tri_header, data_idx);
                #else
                tri_data = *(((global uint4*) g_tri_header) + data_idx); 
                #endif
            }

            // clear its output buffers
            for (int i=get_local_id(0); i < c_num_bins; i += get_local_size(0))
                clear_sub_group_mask(&s_out_mask[get_local_id(1)][i]);
            
            barrier(CLK_LOCAL_MEM_FENCE);
            
            // setup bounding box and edge functions, and rasterize
            int lox, loy, hix, hiy;
            if (local_id < buf_count) {
                int v0x = add_s16lo_s16lo(tri_data.x, c_viewport_width  * (CR_SUBPIXEL_SIZE >> 1));
                int v0y = add_s16hi_s16lo(tri_data.x, c_viewport_height * (CR_SUBPIXEL_SIZE >> 1));
                int d01x = sub_s16lo_s16lo(tri_data.y, tri_data.x);
                int d01y = sub_s16hi_s16hi(tri_data.y, tri_data.x);
                int d02x = sub_s16lo_s16lo(tri_data.z, tri_data.x);
                int d02y = sub_s16hi_s16hi(tri_data.z, tri_data.x);
                int bin_log = CR_BIN_LOG2 + CR_TILE_LOG2 + CR_SUBPIXEL_LOG2;
                lox = add_clamp_0_x((v0x + min_min(d01x, 0, d02x)) >> bin_log, 0, c_width_bins  - 1);
                loy = add_clamp_0_x((v0y + min_min(d01y, 0, d02y)) >> bin_log, 0, c_height_bins - 1);
                hix = add_clamp_0_x((v0x + max_max(d01x, 0, d02x)) >> bin_log, 0, c_width_bins  - 1);
                hiy = add_clamp_0_x((v0y + max_max(d01y, 0, d02y)) >> bin_log, 0, c_height_bins - 1);

                // triangle lies between hix & hiy & lox & loy
                sub_group_mask_t bit;
                clear_sub_group_mask(&bit);
                set_bit_sub_group_mask(&bit, get_sub_group_local_id());

                {
                    // TODO: maybe for 1 bin can be optimized with intrinsicts and avoid atomics.
                    if (hix == lox && hiy == loy) { // only one bin afected
                        int bin_idx = lox + c_width_bins * loy;
                        atomic_or_sub_group_mask(&s_out_mask[get_local_id(1)][bin_idx], bit);
                    } else if ((hix <= lox+1 && hiy <= loy+1)) { // 2x2 bin afected
                        int bin_idx = lox + c_width_bins * loy;
                        atomic_or_sub_group_mask(&s_out_mask[get_local_id(1)][bin_idx], bit);
                        if (hix > lox) atomic_or_sub_group_mask(&s_out_mask[get_local_id(1)][bin_idx + 1], bit);
                        if (hiy > loy) atomic_or_sub_group_mask(&s_out_mask[get_local_id(1)][bin_idx + c_width_bins], bit);
                        if (hix > lox && hiy > loy) atomic_or_sub_group_mask(&s_out_mask[get_local_id(1)][bin_idx + c_width_bins + 1], bit);
                    } else {
                        int d12x = d02x - d01x, d12y = d02y - d01y;
                        v0x -= lox << bin_log, v0y -= loy << bin_log;
                        int t01 = v0x * d01y - v0y * d01x;
                        int t02 = v0y * d02x - v0x * d02y;
                        int t12 = d01x * d12y - d01y * d12x - t01 - t02;
                        int b01 = add_sub(t01 >> bin_log, max(d01x, 0), min(d01y, 0));
                        int b02 = add_sub(t02 >> bin_log, max(d02y, 0), min(d02x, 0));
                        int b12 = add_sub(t12 >> bin_log, max(d12x, 0), min(d12y, 0));

                        int width = hix - lox + 1;
                        d01x += width * d01y;
                        d02x += width * d02y;
                        d12x += width * d12y;

                        local volatile sub_group_mask_t* currPtr = &s_out_mask[get_local_id(1)][lox + loy * c_width_bins];
                        local volatile sub_group_mask_t* skipPtr = &s_out_mask[get_local_id(1)][(hix + 1) + loy * c_width_bins];
                        local volatile sub_group_mask_t* endPtr  = &s_out_mask[get_local_id(1)][lox + (hiy + 1) * c_width_bins];
                        int stride  = c_width_bins;
                        int ptrYInc = stride - width;

                        do
                        {
                            if (b01 >= 0 && b02 >= 0 && b12 >= 0)
                                atomic_or_sub_group_mask(currPtr, bit);
                            currPtr += 1, b01 -= d01y, b02 += d02y, b12 -= d12y;
                            if (currPtr == skipPtr)
                                currPtr += ptrYInc, b01 += d01x, b02 -= d02x, b12 += d12x, skipPtr += stride;
                        }
                        while (currPtr != endPtr);
                    }
                }

            }

            #ifndef DEVICE_BARRIER_SYNC_LOCAL_ATOMIC
            {
                barrier(CLK_LOCAL_MEM_FENCE); // ensure all atomics commits

                sub_group_mask_t empty_mask = get_sub_group_mask_zero();
                
                // commit to local memory calling again atomic with 0
                #pragma unroll
                for(int bin_idx = get_local_id(0); bin_idx < CR_MAXBINS_SQR; bin_idx += DEVICE_SUB_GROUP_THREADS) 
                {
                    s_out_mask[get_local_id(1)][bin_idx] = atomic_or_sub_group_mask(&s_out_mask[get_local_id(1)][bin_idx], empty_mask);
                }
            }
            #endif
            
            // count per-bin contributions
            s_over_total = 0; // overflow counter

            // ensure that out masks are done
            barrier(CLK_LOCAL_MEM_FENCE);

            int over_index = -1;
            {
                int total = 0, ofs;
                bool overflow = 0;

                if (local_id < c_num_bins)
                {
                    for (int sub_group = 0; sub_group < get_num_sub_groups(); ++sub_group) 
                    {
                        total += popcount_sub_group_mask(s_out_mask[sub_group][local_id]);
                        s_out_count[sub_group][local_id] = total;
                    }
                
                    // overflow => request a new segment
                    ofs = s_out_ofs[local_id];
                    // TODO: Checkout this, maybe can be write to be more understandable
                    overflow = ((ofs - 1) >> CR_BIN_SEG_LOG2) != (((ofs - 1) + total) >> CR_BIN_SEG_LOG2);
                }

                // exc cumm scan of all overflows in the work group 
                uint over_total=0;
                uint exc_scan_over_index=0;
                
                exc_scan_over_index = local_1dim_scan_inclusive_add_bool(overflow, l_temp) - overflow;

                if (get_local_id(0) == get_local_size(0)-1)
                    over_total = atomic_add(&s_over_total, exc_scan_over_index + overflow);

                over_total = local_1dim_broadcast(over_total, get_local_size(0)-1, l_temp);

                if (overflow) {
                    over_index = exc_scan_over_index + over_total;
                    s_over_index[local_id] = over_index;
                }
                
            }

            // sync after over_total is ready
            barrier(CLK_LOCAL_MEM_FENCE);

            uint over_total = s_over_total;
            uint alloc_base = 0;
            if (over_total > 0)
            {
                // allocate memory if fits
                if (local_id == 0)
                {
                    uint alloc_base = atomic_add(a_num_bin_segs, over_total);
                    s_alloc_base = (alloc_base + over_total <= c_max_bin_segs) ? alloc_base : 0;
                }
                barrier(CLK_LOCAL_MEM_FENCE);
                alloc_base = s_alloc_base;

                // did my bin overflow?
                if (over_index != -1)
                {
                    // calculate new segment index
                    int seg_idx = alloc_base + over_index;

                    // add to linked list
                    if (s_out_ofs[local_id] < 0)
                    {
                        g_bin_first_seg[(local_id * CR_BIN_STREAMS_SIZE) + get_group_id(0)] = seg_idx;
                    }
                    else
                    {
                        g_bin_seg_next[(s_out_ofs[local_id] - 1) >> CR_BIN_SEG_LOG2] = seg_idx;
                    }

                    // defaults
                    g_bin_seg_next [seg_idx] = -1;
                    g_bin_seg_count[seg_idx] = CR_BIN_SEG_SIZE;
                }
            }

            // concurrent emission -- each warp handles its own triangle
            if (local_id < buf_count)
            {
                int triPos  = (buf_index + local_id) & (FW_ARRAY_SIZE(s_tri_buf) - 1);
                int currBin = lox + loy * c_width_bins;
                int skipBin = (hix + 1) + loy * c_width_bins;
                int endBin  = lox + (hiy + 1) * c_width_bins;
                int binYInc = c_width_bins - (hix - lox + 1);

                // loop over triangle's bins
                do
                {
                    sub_group_mask_t out_mask = s_out_mask[get_local_id(1)][currBin];
                    if (get_bit_sub_group_mask(out_mask, get_sub_group_local_id()))
                    {
                        int idx = popcount_sub_group_mask(and_sub_group_mask(out_mask, get_lane_sub_group_mask_lt()));
                        if (get_local_id(1) > 0)
                            idx += s_out_count[get_local_id(1)-1][currBin];

                        int base = s_out_ofs[currBin];
                        int free = (-base) & (CR_BIN_SEG_SIZE - 1);
                        if (idx >= free)
                            idx += ((alloc_base + s_over_index[currBin]) << CR_BIN_SEG_LOG2) - free;
                        else
                            idx += base;

                        g_bin_seg_data[idx] = s_tri_buf[triPos];
                    }

                    currBin++;
                    if (currBin == skipBin)
                        currBin += binYInc, skipBin += c_width_bins;
                }
                while (currBin != endBin);
            }

            // wait all triangles to finish, then replace overflown segment offsets
            barrier(CLK_LOCAL_MEM_FENCE);
            if (local_id < c_num_bins)
            {
                uint total  = s_out_count[DEVICE_BIN_SUB_GROUPS - 1][local_id];
                uint oldOfs = s_out_ofs[local_id];
                if (over_index == -1)
                    s_out_ofs[local_id] = oldOfs + total;
                else
                {
                    int addr = oldOfs + total;
                    addr = ((addr - 1) & (CR_BIN_SEG_SIZE - 1)) + 1;
                    addr += (alloc_base + over_index) << CR_BIN_SEG_LOG2;
                    s_out_ofs[local_id] = addr;
                }
                s_out_total[local_id] += total;
            }

            // these triangles are now done
            int count = min(buf_count, DEVICE_BIN_SUB_GROUPS * DEVICE_SUB_GROUP_THREADS);
            buf_count -= count;
            buf_index += count;
            buf_index &= FW_ARRAY_SIZE(s_tri_buf)-1;

        } while(buf_count > 0 || batch_pos < batch_end);

        // flush all bins
        if (local_id < c_num_bins) {
            int ofs = s_out_ofs[local_id];
            if (ofs & (CR_BIN_SEG_SIZE-1)) {
                int seg = ofs >> CR_BIN_SEG_LOG2;
                g_bin_seg_count[seg] = ofs & (CR_BIN_SEG_SIZE-1);
                s_out_ofs[local_id] = (ofs - CR_BIN_SEG_SIZE - 1) & -CR_BIN_SEG_SIZE;
            }
        }
    }

    // output totals
    if (local_id < c_num_bins)
    {
        g_bin_total[(local_id * CR_BIN_STREAMS_SIZE) + get_group_id(0)] = s_out_total[local_id];
    }

}