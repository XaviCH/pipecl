#ifdef __COMPILER_RELATIVE_PATH__
#include <backend/types.cl>
#include <backend/utils/sub_group_mask.cl>
#include <backend/utils/sync.cl>
#else
#include "glsc2/src/backend/types.cl"
#include "glsc2/src/backend/utils/sub_group_mask.cl"
#include "glsc2/src/backend/utils/sync.cl"
#endif


inline void extract_tri_data(
    uint4 tri_data, int origin_x, int origin_y, 
    int* v0x, int* v0y, int* d01x, int* d01y, int* d02x, int* d02y
) {
    *v0x = sub_s16lo_s16lo(tri_data.x, origin_x);
    *v0y = sub_s16hi_s16lo(tri_data.x, origin_y);
    *d01x = sub_s16lo_s16lo(tri_data.y, tri_data.x);
    *d01y = sub_s16hi_s16hi(tri_data.y, tri_data.x);
    *d02x = sub_s16lo_s16lo(tri_data.z, tri_data.x);
    *d02y = sub_s16hi_s16hi(tri_data.z, tri_data.x);
}

inline void compute_tile_aabb(
    int v0x, int v0y, int d01x, int d01y, int d02x, int d02y, 
    int tile_log, int max_tile_x_in_bin, int max_tile_y_in_bin,
    int* lox, int* loy, int* hix, int* hiy
) {
    *lox = add_clamp_0_x((v0x + min_min(d01x, 0, d02x)) >> tile_log, 0, max_tile_x_in_bin);
    *loy = add_clamp_0_x((v0y + min_min(d01y, 0, d02y)) >> tile_log, 0, max_tile_y_in_bin);
    *hix = add_clamp_0_x((v0x + max_max(d01x, 0, d02x)) >> tile_log, 0, max_tile_x_in_bin);
    *hiy = add_clamp_0_x((v0y + max_max(d01y, 0, d02y)) >> tile_log, 0, max_tile_y_in_bin);
}

// maybe rm local, function do not require sync
/**
 * @brief emit mask for each 1 dim thread that needs to write into tile 
 * @param s_warp_emit_mask
 */
static inline void local_emit_triangle_mask(
    uint4 tri_data, 
    local sub_group_mask_t (*s_warp_emit_mask)[CR_BIN_SQR + 1],
    local volatile sub_group_mask_t* l_temp, 
    int tri_idx,
    int origin_x, int origin_y,
    int max_tile_x_in_bin, int max_tile_y_in_bin,
    int tile_log
) {
    if (tri_idx == -1) return;

    int v0x, v0y, d01x, d01y, d02x, d02y;
    extract_tri_data(
        tri_data, origin_x, origin_y, 
        &v0x, &v0y, &d01x, &d01y, &d02x, &d02y);

    int lox, loy, hix, hiy;
    compute_tile_aabb(
        v0x, v0y, d01x, d01y, d02x, d02y, 
        tile_log, max_tile_x_in_bin, max_tile_y_in_bin, 
        &lox, &loy, &hix, &hiy);

    int sizex, sizey, area;
    {
        sizex = add_sub(hix, 1, lox);
        sizey = add_sub(hiy, 1, loy);
        area = sizex * sizey;
    }

    local sub_group_mask_t* curr_ptr;
    int ptr_y_inc;
    sub_group_mask_t mask_bit;
    {
        clear_sub_group_mask(&mask_bit);
        curr_ptr = &s_warp_emit_mask[get_local_id(1)][lox + (loy << CR_BIN_LOG2)];
        ptr_y_inc = CR_BIN_SIZE - sizex;
        set_bit_sub_group_mask(&mask_bit, get_local_id(0));
    }

    
    if (sizex <= 2 && sizey <= 2)
    {
        atomic_or_sub_group_mask(curr_ptr, mask_bit);
        if (sizex == 2) atomic_or_sub_group_mask(curr_ptr + 1, mask_bit);
        if (sizey == 2) atomic_or_sub_group_mask(curr_ptr + CR_BIN_SIZE, mask_bit);
        if (sizex == 2 && sizey == 2) atomic_or_sub_group_mask(curr_ptr + 1 + CR_BIN_SIZE, mask_bit);
        return;
    }
    
    // Initialize edge functions.
    int d12x, d12y, b01, b02, b12;
    {
        d12x = d02x - d01x;
        d12y = d02y - d01y;
        v0x -= lox << tile_log;
        v0y -= loy << tile_log;

        int t01 = v0x * d01y - v0y * d01x;
        int t02 = v0y * d02x - v0x * d02y;
        int t12 = d01x * d12y - d01y * d12x - t01 - t02;
        b01 = add_sub(t01 >> tile_log, max(d01x, 0), min(d01y, 0));
        b02 = add_sub(t02 >> tile_log, max(d02y, 0), min(d02x, 0));
        b12 = add_sub(t12 >> tile_log, max(d12x, 0), min(d12y, 0));

        d01x += sizex * d01y;
        d02x += sizex * d02y;
        d12x += sizex * d12y;
    }

    {
        local sub_group_mask_t* skip_ptr = curr_ptr + (sizex);
        local sub_group_mask_t* end_ptr  = curr_ptr + (sizey << CR_BIN_LOG2);
        do
        {
            if (b01 >= 0 && b02 >= 0 && b12 >= 0)
                atomic_or_sub_group_mask(curr_ptr, mask_bit);
            curr_ptr += 1, b01 -= d01y, b02 += d02y, b12 -= d12y;
            if (curr_ptr == skip_ptr)
                curr_ptr += ptr_y_inc, b01 += d01x, b02 -= d02x, b12 += d12x, skip_ptr += CR_BIN_SIZE;
        }
        while (curr_ptr != end_ptr);
    }

}

#if (DEVICE_SUB_GROUP_INTRINSICTS_SUPPORT || DEVICE_SUB_GROUP_LOCKSTEP_RAW_SUPPORT)
inline void sub_group_emit_prefix_sum(
    local sub_group_mask_t (*s_warp_emit_mask)[CR_BIN_SQR + 1],
    local uint (*s_warp_emit_prefix_sum)[CR_BIN_SQR + 1],
    local uint* s_tile_stream_curr_ofs,
    local uint* s_tile_emit_prefix_sum,
    local volatile uint* l_temp,
    int emit_shift
) {

    for (int tile_in_bin_chunk = get_sub_group_id(); tile_in_bin_chunk * get_sub_group_size() < CR_BIN_SQR; tile_in_bin_chunk += get_num_sub_groups()) {
        int tile_in_bin = tile_in_bin_chunk * get_sub_group_size() + get_sub_group_local_id();

        int tile_emits, tile_allocs;
        uint sum = 0;
        if (tile_in_bin < CR_BIN_SQR)
        {
            tile_emits = 0;

            for (int i = 0; i < get_num_sub_groups(); i++)
            {
                tile_emits += popcount_sub_group_mask(s_warp_emit_mask[i][tile_in_bin]);
                s_warp_emit_prefix_sum[i][tile_in_bin] = tile_emits;
            }

            // Determine the number of segments to allocate.

            int space_left = -s_tile_stream_curr_ofs[tile_in_bin] & (CR_TILE_SEG_SIZE - 1);
            tile_allocs = (tile_emits - space_left + CR_TILE_SEG_SIZE - 1) >> CR_TILE_SEG_LOG2;
            sum = (tile_emits << emit_shift) | tile_allocs;
        }

        uint scan_sum;
        #ifdef DEVICE_SUB_GROUP_INTRINSICTS_ENABLED
        {
            if (!any_sub_group_mask(ballot_sub_group_mask(tile_emits >= 2))) {
                sub_group_mask_t lane_mask = get_lane_sub_group_mask_le();
                sub_group_mask_t high = and_sub_group_mask(ballot_sub_group_mask(tile_emits & 1), lane_mask);
                sub_group_mask_t low = and_sub_group_mask(ballot_sub_group_mask(tile_allocs & 1), lane_mask);

                scan_sum = (popcount_sub_group_mask(high) << emit_shift) | popcount_sub_group_mask(low);
            } else {
                scan_sum = sub_group_scan_inclusive_add(sum);
            }
        }
        #else
        {
            if (tile_in_bin < CR_BIN_SQR)    
                scan_sum = local_1dim_scan_inclusive_add(sum, l_temp);
        }
        #endif
        
        if (tile_in_bin < CR_BIN_SQR)
            s_tile_emit_prefix_sum[tile_in_bin + 1] = scan_sum;
    }
}
#endif

static inline void local_emit_prefix_sum(
    local sub_group_mask_t (*s_warp_emit_mask)[CR_BIN_SQR + 1],
    local uint (*s_warp_emit_prefix_sum)[CR_BIN_SQR + 1],
    local uint* s_tile_stream_curr_ofs,
    local uint* s_tile_emit_prefix_sum,
    local volatile uint* l_temp,
    int emit_shift
) {

    /*
    #if (DEVICE_SUB_GROUP_INTRINSICTS_SUPPORT || DEVICE_SUB_GROUP_LOCKSTEP_RAW_SUPPORT)
    {
        sub_group_emit_prefix_sum(s_warp_emit_mask, s_warp_emit_prefix_sum, s_tile_stream_curr_ofs, s_tile_emit_prefix_sum, l_temp, emit_shift);
        return;
    }
    #endif
    */
    // 
    // int emit_shift       = CR_BIN_LOG2 * 2 + DEVICE_SUB_GROUP_THREADS_LOG2; // We scan ((num_emits << emit_shift) | num_allocs) over tiles.


    for (int tile_in_bin_chunk = 0; tile_in_bin_chunk < CR_BIN_SQR; tile_in_bin_chunk += get_local_linear_size()) 
    {
        int tile_in_bin = tile_in_bin_chunk + get_local_linear_id();

        int tile_emits, tile_allocs;

        uint sum = 0;
        
        if (tile_in_bin < CR_BIN_SQR)
        {
            tile_emits = 0;

            for (int i = 0; i < DEVICE_COARSE_SUB_GROUPS; i++)
            {
                tile_emits += popcount_sub_group_mask(s_warp_emit_mask[i][tile_in_bin]);
                s_warp_emit_prefix_sum[i][tile_in_bin] = tile_emits;
            }

            // Determine the number of segments to allocate.

            int space_left = -s_tile_stream_curr_ofs[tile_in_bin] & (CR_TILE_SEG_SIZE - 1);
            tile_allocs = (tile_emits - space_left + CR_TILE_SEG_SIZE - 1) / CR_TILE_SEG_SIZE;
            sum = (tile_emits << emit_shift) | tile_allocs;
        }

        // do it range to avoid unnecessary scan when tile_in_bin >= CR_BIN_SQR
        uint scan_sum = local_scan_inclusive_add(sum, l_temp);
        
        if (tile_in_bin < CR_BIN_SQR)
            s_tile_emit_prefix_sum[tile_in_bin + 1] = scan_sum;
    }
}

static inline void sort_shared(local volatile uint* ptr, int num_items)
{
    int thread_local_id = get_local_linear_id(); // get_local_id(0) + get_local_id(1) * get_local_size(0);
    int range = 16;

    // Use transposition sort within each 16-wide subrange.

    int base = thread_local_id * 2;
    if (base < num_items - 1)
    {
        bool try_odd = (base < num_items - 2 && (~base & (range - 2)) != 0);
        uint mid = ptr[base + 1];

        for (int iter = 0; iter < range; iter += 2)
        {
            // Evens.

            uint tmp = ptr[base + 0];
            if (tmp > mid)
            {
                ptr[base + 0] = mid;
                mid = tmp;
            }

            // Odds.

            if (try_odd)
            {
                tmp = ptr[base + 2];
                if (mid > tmp)
                {
                    ptr[base + 2] = mid;
                    mid = tmp;
                }
            }
        }
        ptr[base + 1] = mid;
    }

    // Multiple subranges => Merge hierarchically.

    for (; range < num_items; range <<= 1)
    {
        // Assuming that we would insert the current item into the other
        // subrange, use binary search to find the appropriate slot.

        barrier(CLK_LOCAL_MEM_FENCE);

        uint item;
        int slot;
        if (thread_local_id < num_items)
        {
            item = ptr[thread_local_id];
            slot = (thread_local_id & -range) ^ range;
            if (slot < num_items)
            {
                uint tmp = ptr[slot];
                bool inclusive = ((thread_local_id & range) != 0);
                if (tmp < item || (inclusive && tmp == item))
                {
                    for (int step = (range >> 1); step != 0; step >>= 1)
                    {
                        int probe = slot + step;
                        if (probe < num_items)
                        {
                            tmp = ptr[probe];
                            if (tmp < item || (inclusive && tmp == item))
                                slot = probe;
                        }
                    }
                    slot++;
                }
            }
        }

        // Store the item at an appropriate place.

        barrier(CLK_LOCAL_MEM_FENCE);

        if (thread_local_id < num_items)
            ptr[slot + (thread_local_id & (range * 2 - 1)) - range] = item;
    }
}


//---

inline int global_tile_idx(int tile_in_bin, int c_width_tiles)
{
    int tile_x = tile_in_bin & (CR_BIN_SIZE - 1);
    int tile_y = tile_in_bin >> CR_BIN_LOG2;
    return tile_x + tile_y * c_width_tiles;
}

//----------------------------------------------------------------------------------------

static inline void local_find_first_stream(
    global const int* restrict g_bin_seg_count,
    global const int* restrict g_bin_seg_next,
    global const int* restrict g_bin_seg_data,
    local volatile int* restrict s_bin_stream_first_tri,
    local volatile int* restrict s_bin_stream_curr_seg,
    local volatile uint* restrict s_bin_stream_selected_ofs,
    local volatile uint* restrict s_bin_stream_selected_size,
    local volatile int* restrict s_tri_queue_write_pos,
    local volatile uint* restrict l_temp,
    const uint tri_queue_write_pos
) {
    uint thread_local_id = get_local_linear_id();

    uint first_tri = UINT_MAX;
    
    if (thread_local_id < CR_BIN_STREAMS_SIZE)
        first_tri = s_bin_stream_first_tri[thread_local_id];

    // TODO: impl this inside a local_1dim, maybe local_reduce_min_scoped(value, l_array, threads)
    uint min_first_tri;

    #if (CR_BIN_STREAMS_SIZE <= DEVICE_SUB_GROUP_THREADS)
    {
        // #if (DEVICE_SUB_GROUP_INTRINSICTS_SUPPORT || DEVICE_SUB_GROUP_LOCKSTEP_RAW_SUPPORT)
        // {
        //     if (thread_local_id >= CR_BIN_STREAMS_SIZE) return;
        // }
        // #endif
        min_first_tri = local_1dim_reduce_min(first_tri, l_temp);
    }
    #else
    {
        min_first_tri = local_reduce_min(first_tri, l_temp);
    }
    #endif

    if (min_first_tri == first_tri && thread_local_id < CR_BIN_STREAMS_SIZE)
    {
        int seg_idx = s_bin_stream_curr_seg[thread_local_id];
        *s_bin_stream_selected_ofs = seg_idx << CR_BIN_SEG_LOG2;
        if (seg_idx != -1)
        {
            int seg_size = g_bin_seg_count[seg_idx];
            int seg_next = g_bin_seg_next[seg_idx];
            *s_bin_stream_selected_size = seg_size;
            *s_tri_queue_write_pos = tri_queue_write_pos + seg_size;
            s_bin_stream_curr_seg[thread_local_id] = seg_next;
            s_bin_stream_first_tri[thread_local_id] = (seg_next == -1) ? ~0u : g_bin_seg_data[seg_next << CR_BIN_SEG_LOG2];
        }
    }
}

//----------------------------------------------------------------------------------------
kernel
#ifdef DEVICE_SUB_GROUP_ENABLED
__attribute__((reqd_work_group_size(DEVICE_SUB_GROUP_THREADS, DEVICE_COARSE_SUB_GROUPS, 1)))
#else
    #error This kernel requires DEVICE_SUB_GROUP_ENABLED
#endif
void coarse_raster(
    volatile global int* a_coarse_counter,
    volatile global int* a_num_active_tiles,
    global const int* a_num_bin_segs,
    global int* a_num_tile_segs,
    global const int* a_num_subtris,

    global int* g_active_tiles,
    global const int* g_bin_first_seg,    
    global const int* g_bin_seg_count,
    global const int* g_bin_seg_data,
    global const int* g_bin_seg_next,
    global const int* g_bin_total,
    global int* g_tile_first_seg,
    global int* g_tile_seg_count,
    global int* g_tile_seg_data,
    global int* g_tile_seg_next,
    global const triangle_header_t*  g_tri_header,

    #ifdef DEVICE_IMAGE_ENABLED
    read_only image1d_buffer_t t_tri_header,
    #endif

    const int c_deferred_clear,
    const int c_height_tiles,
    const int c_max_bin_segs,
    const int c_max_subtris,
    const int c_max_tile_segs,
    const int c_num_bins,
    const int c_viewport_height,
    const int c_viewport_width,
    const int c_width_bins,
    const int c_width_tiles
) {
    
    local volatile uint s_work_counter;

    local volatile uint s_bin_order           [CR_MAXBINS_SQR];                   // 1KB
    local volatile int s_bin_stream_curr_seg  [CR_BIN_STREAMS_SIZE];              // 0KB
    local volatile int s_bin_stream_first_tri [CR_BIN_STREAMS_SIZE];              // 0KB
    local volatile int s_tri_queue            [CR_COARSE_QUEUE_SIZE];             // 4KB
    local volatile int s_tri_queue_write_pos;
    local volatile uint s_bin_stream_selected_ofs;
    local volatile uint s_bin_stream_selected_size;

    local volatile sub_group_mask_t s_warp_emit_mask [DEVICE_COARSE_SUB_GROUPS][CR_BIN_SQR + 1];  // 16KB, +1 to avoid bank collisions
    local volatile uint s_warp_emit_prefix_sum [DEVICE_COARSE_SUB_GROUPS][CR_BIN_SQR + 1];  // 16KB, +1 to avoid bank collisions
    local volatile uint s_tile_emit_prefix_sum [CR_BIN_SQR + 1];                   // 1KB, zero at the beginning
    local volatile uint s_tile_alloc_prefix_sum [CR_BIN_SQR + 1];                   // 1KB, zero at the beginning
    local volatile int s_tile_stream_curr_ofs [CR_BIN_SQR];                       // 1KB
    local volatile uint s_first_alloc_seg;
    local volatile uint s_first_active_idx;

    local volatile uint l_temp [DEVICE_COARSE_SUB_GROUPS*DEVICE_SUB_GROUP_THREADS];

    // Private
    int tile_log         = CR_TILE_LOG2 + CR_SUBPIXEL_LOG2;
    int thread_local_id  = get_local_linear_id();
    int emit_shift       = CR_BIN_LOG2 * 2 + DEVICE_SUB_GROUP_THREADS_LOG2; // We scan ((num_emits << emit_shift) | num_allocs) over tiles.

    if (*a_num_subtris > c_max_subtris || *a_num_bin_segs > c_max_bin_segs)
        return;

    // Initialize sharedmem arrays.

    s_tile_emit_prefix_sum[0] = 0;
    s_tile_alloc_prefix_sum[0] = 0;

    // Sort bins in descending order of triangle count.

    for (int bin_idx = thread_local_id; bin_idx < c_num_bins; bin_idx += get_local_linear_size())
    {
        int count = 0;
        for (int i = 0; i < CR_BIN_STREAMS_SIZE; i++)
            count += g_bin_total[(bin_idx * CR_BIN_STREAMS_SIZE) + i];
        // n = used_bins, k = num_bits_to_fit_bin_idx 
        // bin_order[k:0] = bin_idx
        // count = num_triangles_in_total_for_bin_idx
        // bin_order[n:k] = negate-1 the number of triangles
        s_bin_order[bin_idx] = (~count << (CR_MAXBINS_LOG2 * 2)) | bin_idx;
    }

    barrier(CLK_LOCAL_MEM_FENCE);
    sort_shared(s_bin_order, c_num_bins);

    // Process each bin by one block.

    for (;;)
    {
        // Pick a bin for the block.

        if (thread_local_id == 0)
            s_work_counter = atomic_add(a_coarse_counter, 1);
        barrier(CLK_LOCAL_MEM_FENCE);

        int work_counter = s_work_counter;
        if (work_counter >= c_num_bins)
        {
            break;
        }

        uint bin_order = s_bin_order[work_counter];
        bool bin_empty = ((~bin_order >> (CR_MAXBINS_LOG2 * 2)) == 0);
        if (bin_empty && !c_deferred_clear)
        {
            break;
        }

        int bin_idx = bin_order & (CR_MAXBINS_SQR - 1);

        // Initialize input/output streams.

        int tri_queue_write_pos = 0;
        int tri_queue_read_pos = 0;

        if (thread_local_id < CR_BIN_STREAMS_SIZE)
        {
            int seg_idx = g_bin_first_seg[(bin_idx * CR_BIN_STREAMS_SIZE) + thread_local_id];
            s_bin_stream_curr_seg[thread_local_id] = seg_idx;
            s_bin_stream_first_tri[thread_local_id] = (seg_idx == -1) ? ~0u : g_bin_seg_data[seg_idx << CR_BIN_SEG_LOG2];
        }

        for (int tile_in_bin = thread_local_id; tile_in_bin < CR_BIN_SQR; tile_in_bin += get_local_linear_size())
            s_tile_stream_curr_ofs[tile_in_bin] = -CR_TILE_SEG_SIZE;

        // Initialize per-bin state.

        int bin_y = bin_idx / c_width_bins; // idiv_fast(bin_idx, c_width_bins);
        int bin_x = bin_idx - bin_y * c_width_bins;
        int origin_x = (bin_x << (CR_BIN_LOG2 + tile_log)) - (c_viewport_width << (CR_SUBPIXEL_LOG2 - 1));
        int origin_y = (bin_y << (CR_BIN_LOG2 + tile_log)) - (c_viewport_height << (CR_SUBPIXEL_LOG2 - 1));
        int max_tile_x_in_bin = min(c_width_tiles - (bin_x << CR_BIN_LOG2), CR_BIN_SIZE) - 1;
        int max_tile_y_in_bin = min(c_height_tiles - (bin_y << CR_BIN_LOG2), CR_BIN_SIZE) - 1;
        int bin_tile_idx = (bin_x + bin_y * c_width_tiles) << CR_BIN_LOG2;


        // Entire block: Merge input streams and process triangles.
        if (!bin_empty)
        do
        {
            //------------------------------------------------------------------------
            // Merge.
            //------------------------------------------------------------------------

            // Entire block: Not enough triangles => merge and queue segments.
            // NOTE: The bin exit criterion assumes that we queue more triangles than we actually need.

            while (tri_queue_write_pos - tri_queue_read_pos <= get_local_linear_size())
            {
                // First warp: Choose the segment with the lowest initial triangle index.
                // Find the stream with the lowest triangle index.

                local_find_first_stream(
                    g_bin_seg_count,
                    g_bin_seg_next,
                    g_bin_seg_data,
                    s_bin_stream_first_tri,
                    s_bin_stream_curr_seg,
                    &s_bin_stream_selected_ofs,
                    &s_bin_stream_selected_size,
                    &s_tri_queue_write_pos,
                    l_temp,
                    tri_queue_write_pos
                );

                // No more segments => break.

                barrier(CLK_LOCAL_MEM_FENCE);
                tri_queue_write_pos = s_tri_queue_write_pos;
                int seg_ofs = s_bin_stream_selected_ofs;

                if (seg_ofs < 0) break;

                int seg_size = s_bin_stream_selected_size;
                barrier(CLK_LOCAL_MEM_FENCE); // ensure reads from local mem

                // Fetch triangles into the queue.
                for (int idx_in_seg = thread_local_id; idx_in_seg < seg_size; idx_in_seg += get_local_linear_size())
                {
                    int tri_idx = g_bin_seg_data[seg_ofs + idx_in_seg];
                    s_tri_queue[(tri_queue_write_pos - seg_size + idx_in_seg) & (CR_COARSE_QUEUE_SIZE - 1)] = tri_idx;
                }

            }
            // All threads: Clear emit masks.

            for (int mask_idx = thread_local_id; mask_idx < get_local_size(1) * CR_BIN_SQR; mask_idx += get_local_linear_size())
                clear_sub_group_mask(&s_warp_emit_mask[mask_idx / CR_BIN_SQR][mask_idx % CR_BIN_SQR]);

            barrier(CLK_LOCAL_MEM_FENCE);

            //------------------------------------------------------------------------
            // Raster.
            //------------------------------------------------------------------------

            // Triangle per thread: Read from the queue.

            int tri_idx = -1;
            if (tri_queue_read_pos + thread_local_id < tri_queue_write_pos)
                tri_idx = s_tri_queue[(tri_queue_read_pos + thread_local_id) & (CR_COARSE_QUEUE_SIZE - 1)];

            uint4 tri_data = (uint4){0, 0, 0, 0};
            if (tri_idx != -1)
            {
                int data_idx = tri_idx >> 3;
                int subtri_idx = tri_idx & 0x7;
                if (subtri_idx != 7)
                    data_idx = g_tri_header[data_idx].misc.misc + subtri_idx;
                
                #ifdef DEVICE_IMAGE_ENABLED
                    tri_data = read_imageui(t_tri_header, data_idx);
                #else
                    tri_data = *(((global uint4*) g_tri_header) + data_idx); 
                #endif
            }

            // Triangle per thread: Record emits (= tile intersections).
            
            local_emit_triangle_mask(
                tri_data, 
                (local sub_group_mask_t(*)[CR_BIN_SQR+1])s_warp_emit_mask, 
                (volatile local sub_group_mask_t*) l_temp, 
                tri_idx, origin_x, origin_y, 
                max_tile_x_in_bin, max_tile_y_in_bin, tile_log);

            #ifndef DEVICE_BARRIER_SYNC_LOCAL_ATOMIC
            {
                barrier(CLK_LOCAL_MEM_FENCE); // ensure all atomics commits

                sub_group_mask_t empty_mask = get_sub_group_mask_zero();

                // commit to local memory calling again atomic with 0
                #pragma unroll
                for(int tile_in_bin = get_local_id(0); tile_in_bin < CR_BIN_SQR; tile_in_bin += DEVICE_SUB_GROUP_THREADS) 
                {
                    s_warp_emit_mask[get_local_id(1)][tile_in_bin] = atomic_or_sub_group_mask(&s_warp_emit_mask[get_local_id(1)][tile_in_bin], empty_mask);
                }
            }
            #endif

            barrier(CLK_LOCAL_MEM_FENCE);
            
            //------------------------------------------------------------------------
            // Count.
            //------------------------------------------------------------------------

            // Tile per thread: Initialize prefix sums.
            
            local_emit_prefix_sum(
                (local sub_group_mask_t(*)[CR_BIN_SQR+1]) s_warp_emit_mask,
                (local uint(*)[CR_BIN_SQR+1]) s_warp_emit_prefix_sum,
                (local uint*) s_tile_stream_curr_ofs,
                (local uint*) s_tile_emit_prefix_sum,
                l_temp,
                emit_shift
                );

            // First warp: Scan-8.

            barrier(CLK_LOCAL_MEM_FENCE);

            // Tile per thread: Finalize prefix sums.
            // Single thread: Allocate segments.
            for (int tile_in_bin = thread_local_id; tile_in_bin < CR_BIN_SQR; tile_in_bin += DEVICE_COARSE_THREADS)
            {
                int sum = s_tile_emit_prefix_sum[tile_in_bin + 1];
                int num_emits = sum >> emit_shift;
                int num_allocs = sum & ((1 << emit_shift) - 1);
                s_tile_emit_prefix_sum[tile_in_bin + 1] = num_emits;
                s_tile_alloc_prefix_sum[tile_in_bin + 1] = num_allocs;

                if (tile_in_bin == CR_BIN_SQR - 1 && num_allocs != 0)
                {
                    int t = atomic_add(a_num_tile_segs, num_allocs);
                    s_first_alloc_seg = (t + num_allocs <= c_max_tile_segs) ? t : 0;
                }
            }

            barrier(CLK_LOCAL_MEM_FENCE);
            int first_alloc_seg   = s_first_alloc_seg;
            int total_emits      = s_tile_emit_prefix_sum[CR_BIN_SQR];
            int total_allocs     = s_tile_alloc_prefix_sum[CR_BIN_SQR];

            //------------------------------------------------------------------------
            // Emit.
            //------------------------------------------------------------------------

            // Emit per thread: Write triangle index to globalmem.

            for (int emit_in_bin = thread_local_id; emit_in_bin < total_emits; emit_in_bin += get_local_linear_size())
            {

                // Find tile in bin.

                local uint* tile_base = (local uint*) &s_tile_emit_prefix_sum[0];
                int tile_in_bin = 0;

                #pragma unroll
                for (int i=CR_BIN_SQR; i>0; i /=2) {
                    int tile_tmp = tile_in_bin + i/2;
                    // ptr = tile_ptr + i/2;
                    if (emit_in_bin >= tile_base[tile_tmp]) {
                        tile_in_bin = tile_tmp;
                        // tile_ptr = ptr;
                    }
                }

                int emit_in_tile = emit_in_bin - tile_base[tile_in_bin];

                // Find warp in tile.

                int warp_step = (CR_BIN_SQR + 1);
                local uint* warp_base = (local uint*)&s_warp_emit_prefix_sum[0][tile_in_bin] - warp_step;
                int warp_in_bin = 0;

                #pragma unroll
                for (int i=DEVICE_COARSE_SUB_GROUPS; i>0; i /=2) {
                    int warp_tmp = warp_in_bin + i/2 * warp_step; 
                    // ptr = warp_ptr + i/2 * warp_step;
                    if (emit_in_tile >= warp_base[warp_tmp]) {
                        // warp_ptr = ptr;
                        warp_in_bin = warp_tmp;
                    }
                }

                int warp_in_tile = warp_in_bin >> (CR_BIN_LOG2 * 2);
                sub_group_mask_t emit_mask = s_warp_emit_mask[0][tile_in_bin + warp_in_bin];
                int emit_in_warp = emit_in_tile - warp_base[warp_in_bin + warp_step] + popcount_sub_group_mask(emit_mask);
                // Find thread in warp.

                int thread_in_warp = 0;
                int pop;
                #pragma unroll
                for(int bits = DEVICE_SUB_GROUP_THREADS/2; bits > 1; bits/=2) 
                {
                    // uint mask = (1u << bits) - 1;
                    sub_group_mask_t mask = get_sub_group_mask_ones_lt(bits);
                    pop = popcount_sub_group_mask(and_sub_group_mask(emit_mask, mask));
                    if (emit_in_warp >= pop) {
                        emit_in_warp -= pop;
                        // emit_mask >>= bits;
                        emit_mask = shift_right_sub_group_mask(emit_mask, bits);
                        thread_in_warp += bits;
                    }
                }
                
                if (emit_in_warp >= get_bit_sub_group_mask(emit_mask, 0))
                    thread_in_warp++;

                // Figure out where to write.

                int curr_ofs = s_tile_stream_curr_ofs[tile_in_bin];
                int space_left = -curr_ofs & (CR_TILE_SEG_SIZE - 1);
                int outOfs = emit_in_tile;

                if (outOfs < space_left)
                    outOfs += curr_ofs;
                else
                {
                    int alloc_lo = first_alloc_seg + s_tile_alloc_prefix_sum[tile_in_bin];
                    outOfs += (alloc_lo << CR_TILE_SEG_LOG2) - space_left;
                }

                // Write.

                int queue_idx = warp_in_tile * DEVICE_SUB_GROUP_THREADS + thread_in_warp;
                int tri_idx = s_tri_queue[(tri_queue_read_pos + queue_idx) & (CR_COARSE_QUEUE_SIZE - 1)];

                g_tile_seg_data[outOfs] = tri_idx;
            }

            //------------------------------------------------------------------------
            // Patch.
            //------------------------------------------------------------------------

            // Allocated segment per thread: Initialize next-pointer and count.

            for (int i = thread_local_id; i < total_allocs; i += get_local_linear_size())
            {
                int seg_idx = first_alloc_seg + i;
                g_tile_seg_next[seg_idx] = seg_idx + 1;
                g_tile_seg_count[seg_idx] = CR_TILE_SEG_SIZE;
            }

            // Tile per thread: Fix previous segment's next-pointer and update s_tile_stream_curr_ofs.
            barrier(CLK_LOCAL_MEM_FENCE);
            for (int tile_in_bin = thread_local_id; tile_in_bin < CR_BIN_SQR; tile_in_bin += get_local_linear_size())
            {
                int old_ofs = s_tile_stream_curr_ofs[tile_in_bin];
                int new_ofs = old_ofs + s_warp_emit_prefix_sum[DEVICE_COARSE_SUB_GROUPS - 1][tile_in_bin];
                int alloc_lo = s_tile_alloc_prefix_sum[tile_in_bin];
                int alloc_hi = s_tile_alloc_prefix_sum[tile_in_bin + 1];

                if (alloc_lo != alloc_hi)
                {
                    global int* next_ptr = &g_tile_seg_next[(old_ofs - 1) >> CR_TILE_SEG_LOG2];
                    if (old_ofs < 0)
                        next_ptr = &g_tile_first_seg[bin_tile_idx + global_tile_idx(tile_in_bin, c_width_tiles)];
                    *next_ptr = first_alloc_seg + alloc_lo;

                    new_ofs--;
                    new_ofs &= CR_TILE_SEG_SIZE - 1;
                    new_ofs |= (first_alloc_seg + alloc_hi - 1) << CR_TILE_SEG_LOG2;
                    new_ofs++;
                }
                s_tile_stream_curr_ofs[tile_in_bin] = new_ofs;
            }

            // Advance queue read pointer.
            // Queue became empty => bin done.

            tri_queue_read_pos += get_local_linear_size();
        }
        while (tri_queue_read_pos < tri_queue_write_pos);

        // Tile per thread: Fix next-pointer and count of the last segment.
        // 32 tiles per warp: Count active tiles.

        barrier(CLK_LOCAL_MEM_FENCE);
        
        // TODO: break dependency
        #if CR_BIN_SQR > DEVICE_COARSE_THREADS
            #error CR_BIN_SQR <= DEVICE_COARSE_THREADS
        #endif
        for (int tile_in_bin_chunk = 0; tile_in_bin_chunk < CR_BIN_SQR; tile_in_bin_chunk += DEVICE_COARSE_THREADS)
        {
            int tile_in_bin = tile_in_bin_chunk + thread_local_id;
            bool active = tile_in_bin < CR_BIN_SQR;

            int ofs, tile_x, tile_y;
            bool force;
            
            ofs = s_tile_stream_curr_ofs[tile_in_bin];
            tile_x = tile_in_bin & (CR_BIN_SIZE - 1);
            tile_y = tile_in_bin >> CR_BIN_LOG2;
            force = (c_deferred_clear && tile_x <= max_tile_x_in_bin && tile_y <= max_tile_y_in_bin);

            int active_tiles = local_scan_inclusive_add_bool((ofs >= 0 || force) && active, l_temp);

            if (thread_local_id == DEVICE_COARSE_THREADS - 1)
                s_first_active_idx = atomic_add(a_num_active_tiles, active_tiles);

            barrier(CLK_LOCAL_MEM_FENCE);

            if (!active) continue;

            int seg_idx = (ofs - 1) >> CR_TILE_SEG_LOG2;
            int seg_count = ofs & (CR_TILE_SEG_SIZE - 1);

            if (ofs >= 0)
                g_tile_seg_next[seg_idx] = -1;
            else if (force)
            {
                s_tile_stream_curr_ofs[tile_in_bin] = 0;
                g_tile_first_seg[bin_tile_idx + tile_x + tile_y * c_width_tiles] = -1;
            }

            if (seg_count != 0)
                g_tile_seg_count[seg_idx] = seg_count;

            if (ofs >= 0 || force)
                g_active_tiles[s_first_active_idx + active_tiles - 1] = bin_tile_idx + global_tile_idx(tile_in_bin, c_width_tiles);
        }

    }

}