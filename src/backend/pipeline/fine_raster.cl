#ifdef __COMPILER_RELATIVE_PATH__
    #include <backend/types.cl>
    #include <backend/utils/sub_group_mask.cl>
    #include <backend/utils/sync.cl>
    #include <backend/pipeline/gl/blending.cl>
    #include <backend/pipeline/gl/depth.cl>
    #include <backend/pipeline/gl/stencil.cl>
    #include <backend/pipeline/gl/clear.cl>
#else
    #include "glsc2/src/backend/types.cl"
    #include "glsc2/src/backend/utils/sub_group_mask.cl"
    #include "glsc2/src/backend/utils/sync.cl"
    #include "glsc2/src/backend/pipeline/gl/blending.cl"
    #include "glsc2/src/backend/pipeline/gl/depth.cl"
    #include "glsc2/src/backend/pipeline/gl/stencil.cl"
    #include "glsc2/src/backend/pipeline/gl/clear.cl"
#endif

#ifndef SHADER
    #ifdef __COMPILER_RELATIVE_PATH__
        #include <backend/pipeline/gl/base.cl>
    #else
        #include "glsc2/src/backend/pipeline/gl/base.cl"
    #endif
#endif



//------------------------------------------------------------------------
// Fragment Shader Interface Wrapper.
//------------------------------------------------------------------------


inline float3 compute_barys(
    const int3* wpleq, const int3* upleq, const int3* vpleq,
    int sample_x, int sample_y)
{
    float w = 1.0f / (float){wpleq->x * sample_x + wpleq->y * sample_y + wpleq->z};
    float u = w * (float){upleq->x * sample_x + upleq->y * sample_y + upleq->z};
    float v = w * (float){vpleq->x * sample_x + vpleq->y * sample_y + vpleq->z};
    return (float3){1.0f - u - v, u, v};
}

//------------------------------------------------------------------------

inline bool run_fragment_shader(
    global void* gl_uniforms,
    FS_KERNEL_PARAMS

    fragment_shader_output_t* output,
    int data_idx, int pixel_x, int pixel_y,
    #ifdef DEVICE_IMAGE_ENABLED
    image1d_buffer_t t_tri_data,
    #else
    global const triangle_data_t* g_tri_data, 
    #endif
    ro_vertex_buffer_t vertex_buffer
) {
    // Fetch primitive data.
    uint4 t1, t2, t3;
    
    // TODO: check this
    // if (data_idx < 0) return false; // invalid triangle, can happen when tile_seg_count is larger than actual segments assigned to the tile

    #ifdef DEVICE_IMAGE_ENABLED
    {
        t1 = read_imageui(t_tri_data, data_idx * 4 + 1); // wx, wy, wb, ux
        t2 = read_imageui(t_tri_data, data_idx * 4 + 2); // uy, ub, vx, vy
        t3 = read_imageui(t_tri_data, data_idx * 4 + 3); // vb, vi0, vi1, vi2
    }
    #else
    {
        t1 = ((global const uint4*) g_tri_data)[data_idx * 4 + 1];
        t2 = ((global const uint4*) g_tri_data)[data_idx * 4 + 2];
        t3 = ((global const uint4*) g_tri_data)[data_idx * 4 + 3];
    }
    #endif
    
    // Pixel varying data.
    int3 wpleq = (int3){t1.x, t1.y, t1.z};
    int3 upleq = (int3){t1.w, t2.x, t2.y};
    int3 vpleq = (int3){t2.z, t2.w, t3.x};
    uint3 vert_idx = (uint3){t3.y, t3.z, t3.w};
    float3 bary = compute_barys(&wpleq, &upleq, &vpleq, (pixel_x * 2 + 1), (pixel_y * 2 + 1));

    return gl_fragment_shader(
        gl_uniforms,
        FS_KERNEL_ARGS
        output, vertex_buffer, vert_idx, bary 
        );
}

//----------------------------------------------
// Blending Shader
//----------------------------------------------


typedef struct {
    bool needs_dst; 
} blend_shader_input_t;

typedef struct {
    bool write_color;
    uint color;
} blend_shader_output_t;

inline bool bs_needs_dst(uint c_render_mode_flags, uint c_blender_op) {
    if ((c_render_mode_flags & RENDER_MODE_FLAG_ENABLE_BLENDER) != 0) {
        return true;
    }
    return false; 
}

inline void run_blend_shader(
    blend_shader_output_t* output, uint src, uint dst,
    uint c_blending_color, uint c_blending_data, uint c_render_mode_flags)
{
    if ((c_render_mode_flags & RENDER_MODE_FLAG_ENABLE_BLENDER) != 0) {
        output->color = blend(src, dst, c_blending_color, c_blending_data);
    } else {
        output->color = src;
    }
    output->write_color = true;
}



//------------------------------------------------------------------------

inline void get_triangle(
    int* tri_idx, int* data_idx, uint4* tri_header, int* segment, 
    global const triangle_header_t* g_tri_header,
    global const int* g_tile_seg_data,
    global const int* g_tile_seg_next,
    global const int* g_tile_seg_count
    #ifdef DEVICE_IMAGE_ENABLED
    , image1d_buffer_t t_tri_header
    #endif
)
{

    if (get_local_id(0) >= g_tile_seg_count[*segment])
    {
        *tri_idx = -1;
        *data_idx = -1;
    }
    else
    {
        int subtri_idx = g_tile_seg_data[*segment * CR_TILE_SEG_SIZE + get_local_id(0)];
        *tri_idx = subtri_idx >> 3;
        *data_idx = *tri_idx;
        subtri_idx &= 7;
        if (subtri_idx != 7)
            *data_idx = g_tri_header[*tri_idx].misc.misc + subtri_idx;
        #ifdef DEVICE_IMAGE_ENABLED
        *tri_header = read_imageui(t_tri_header, *data_idx);
        #else
        *tri_header = ((global const uint4*) g_tri_header)[*data_idx];
        #endif
    }

    // advance to next segment
    *segment = g_tile_seg_next[*segment];
}

//------------------------------------------------------------------------


inline bool early_z_cull(uint render_mode_flags, uint4 tri_header, ushort tile_z_min, ushort tile_z_max, uint c_depth_data)
{
    if ((render_mode_flags & RENDER_MODE_FLAG_ENABLE_DEPTH) != 0 && (render_mode_flags & RENDER_MODE_FLAG_ENABLE_STENCIL) != 0)
    {
        ushort z = (tri_header.w & 0xFFFFF000) >> 16;
        return !range_depth_test(z, tile_z_min, tile_z_max, c_depth_data);
    }
    return false;
}
/*
inline bool early_z_cull(uint render_mode_flags, uint4 tri_header, ushort tile_z_min, ushort tile_z_max, uint c_depth_data)
{
    if ((render_mode_flags & RENDER_MODE_FLAG_ENABLE_DEPTH) != 0)
    {
        ushort zmin = (tri_header.w & 0xFFFFF000) >> 16;
        if (!depth_test(zmin, tile_z_max, c_depth_data))
            return true;
    }
    return false;
}*/

//------------------------------------------------------------------------

inline ulong triangle_pixel_coverage(const int samples_log_2, const uint4 tri_header, int tile_x, int tile_y, local volatile ulong* s_cover8x8_lut, 
    int c_viewport_width, int c_viewport_height)
{
    int base_x = (tile_x << (CR_TILE_LOG2 + CR_SUBPIXEL_LOG2)) - ((c_viewport_width  - 1) << (CR_SUBPIXEL_LOG2 - 1));
    int base_y = (tile_y << (CR_TILE_LOG2 + CR_SUBPIXEL_LOG2)) - ((c_viewport_height - 1) << (CR_SUBPIXEL_LOG2 - 1));

    // extract S16 vertex positions while subtracting tile coordinates
    int v0x  = sub_s16lo_s16lo(tri_header.x, base_x);
    int v0y  = sub_s16hi_s16lo(tri_header.x, base_y);
    int v01x = sub_s16lo_s16lo(tri_header.y, tri_header.x);
    int v01y = sub_s16hi_s16hi(tri_header.y, tri_header.x);
    int v20x = sub_s16lo_s16lo(tri_header.x, tri_header.z);
    int v20y = sub_s16hi_s16hi(tri_header.x, tri_header.z);

    // extract flipbits
    uint f01 = (tri_header.w >> 6) & 0x3C;
    uint f12 = (tri_header.w >> 2) & 0x3C;
    uint f20 = (tri_header.w << 2) & 0x3C;

    // compute per-edge coverage masks
    ulong c01, c12, c20;
    if (samples_log_2 == 0)
    {
        c01 = cover8x8_exact_fast(v0x, v0y, v01x, v01y, f01, s_cover8x8_lut);
        c12 = cover8x8_exact_fast(v0x + v01x, v0y + v01y, -v01x - v20x, -v01y - v20y, f12, s_cover8x8_lut);
        c20 = cover8x8_exact_fast(v0x, v0y, v20x, v20y, f20, s_cover8x8_lut);
    }
    else
    {
        c01 = cover8x8_conservative_fast(v0x, v0y, v01x, v01y, f01, s_cover8x8_lut);
        c12 = cover8x8_conservative_fast(v0x + v01x, v0y + v01y, -v01x - v20x, -v01y - v20y, f12, s_cover8x8_lut);
        c20 = cover8x8_conservative_fast(v0x, v0y, v20x, v20y, f20, s_cover8x8_lut);
    }

    // combine masks
    return c01 & c12 & c20;
}


//------------------------------------------------------------------------

// template <class BlendShaderClass>
inline sub_group_mask_t determine_ROP_lane_mask(uint c_render_mode_flags) //, local volatile uint* warp_temp) mask of lanes that should process an earlier fragment than this lane
{
    /*
    bool reverse_lanes = true;
    if ((c_render_mode_flags & RENDER_MODE_FLAG_ENABLE_DEPTH) == 0)
    {
        if (!bs_needs_dst(c_render_mode_flags, 0))
            reverse_lanes = false;
    }
    */
    bool reverse_lanes = true;
    return reverse_lanes ? get_lane_sub_group_mask_lt() : not_sub_group_mask(get_lane_sub_group_mask_le());
}

static inline int find_bit(ulong mask, int idx)
{
    uint x = ugetLo(mask);
    int  pop = popcount(x);
    bool p   = (pop <= idx);
    if (p) x = ugetHi(mask);
    if (p) idx -= pop;
    int bit = p ? 32 : 0;

    pop = popcount(x & 0x0000ffffu);
    p   = (pop <= idx);
    if (p) x >>= 16;
    if (p) bit += 16;
    if (p) idx -= pop;

    uint tmp = x & 0x000000ffu;
    pop = popcount(tmp);
    p   = (pop <= idx);
    if (p) tmp = x & 0x0000ff00u;
    if (p) idx -= pop;

    return findLeadingOne(tmp) + bit - idx;
}

inline ulong quad_coverage(ulong mask)
{
    mask |= mask >> 1;
    mask |= mask >> 8;
    return mask & 0x0055005500550055;
}


inline int num_fragments(uint render_mode_flags, ulong coverage)
{
    if ((render_mode_flags & RENDER_MODE_FLAG_ENABLE_QUADS) == 0)
        return popcount(coverage);
    else
        return popcount(quad_coverage(coverage)) << 2;
}

inline int find_fragment(ulong coverage, int frag_idx)
{
    return find_bit(coverage, frag_idx);
}

//------------------------------------------------------------------------
// Single-sample implementation.
//------------------------------------------------------------------------

inline void execute_ROP_single_sample(
    const fragment_shader_output_t fs_out, ushort depth, 
    local volatile uint* restrict ptr_color, 
    local volatile ushort* restrict ptr_depth, 
    local volatile uchar* restrict ptr_stencil,
    rop_config_t rop_config
)
{
    blend_shader_input_t blend_shader_input;
    blend_shader_output_t blend_shader_output;

    uint color = float4_to_uint(fs_out.gl_FragColor);

    // per-fragment enabled operations
    bool stencil_test_enabled = is_render_mode_flag_enable_stencil(rop_config.render_mode);
    bool depth_test_enabled = is_render_mode_flag_enable_depth(rop_config.render_mode);

    // TODO: Optimize, ordering on primitive is not always required.
            
    // stencil test
    uchar stencil = *ptr_stencil;
    
    if (stencil_test_enabled) {
        if (!stencil_test(stencil, rop_config.stencil_data)) {
            stencil_operation(ptr_stencil, rop_config.stencil_data, get_stencil_operation_sfail(rop_config.stencil_data));
            return;
        }
    }

    // depth test
    if (depth_test_enabled) {
        if (!depth_test(depth, *ptr_depth, rop_config.depth_data)) {
            if (stencil_test_enabled)
                stencil_operation(ptr_stencil, rop_config.stencil_data, get_stencil_operation_dfail(rop_config.stencil_data));
            return;
        }
        if (get_enabled_depth_data(rop_config.enabled_data)) *ptr_depth = depth;
    }

    // stencil op post depth test
    if (stencil_test_enabled) {
        stencil_operation(ptr_stencil, rop_config.stencil_data, get_stencil_operation_dpass(rop_config.stencil_data));
    }
    
    // blending
    run_blend_shader(&blend_shader_output, color, *ptr_color,
        rop_config.blending_color.misc, rop_config.blending_data.misc, rop_config.render_mode.flags);
    if (blend_shader_output.write_color) {
        cl_uint enabled_color_mask = 0;
        if (get_enabled_red_data(rop_config.enabled_data)) enabled_color_mask |= 0x000000FF;
        if (get_enabled_green_data(rop_config.enabled_data)) enabled_color_mask |= 0x0000FF00;
        if (get_enabled_blue_data(rop_config.enabled_data)) enabled_color_mask |= 0x00FF0000;
        if (get_enabled_alpha_data(rop_config.enabled_data)) enabled_color_mask |= 0xFF000000;

        *ptr_color = (*ptr_color & ~enabled_color_mask) | (enabled_color_mask & blend_shader_output.color);
    }
    
    return;
    
}

//------------------------------------------------------------------------

#ifndef FS_KERNEL_ARGS
#define FS_KERNEL_ARGS
#endif // FS_KERNEL_ARGS

#ifndef FS_KERNEL_PARAMS
#define FS_KERNEL_PARAMS
#endif // FS_KERNEL_PARAMS

static inline bool is_surface_out_viewport(int2 surf, uint2 viewport) 
{
    return surf.x >= viewport.x || surf.y >= viewport.y;
}

static inline int2 get_2d_surface_from_tile(int2 tile, uint pixel)
{
    return (tile << CR_TILE_LOG2) + (int2){
        (pixel & (CR_TILE_SIZE - 1)),
        (pixel >> CR_TILE_LOG2)
    };
}

static inline void local_1dim_load_and_clean_framebuffer_to_local_mem(
    colorbuffer_t colorbuffer, depthbuffer_t depthbuffer, stencilbuffer_t stencilbuffer,
    local uint* restrict l1_color, local ushort* restrict l1_depth, local uchar* restrict l1_stencil,
    gl_framebuffer_data_t framebuffer_data,
    const ulong  clear_write_values, const enabled_data_t clear_enabled_data,
    const uint   colorbuffer_mode,
    int2 tile, uint2 viewport
) {
    bool load_colorbuffer, load_depthbuffer, load_stencilbuffer;

    load_colorbuffer =
        is_framebuffer_data_colorbuffer_enabled(framebuffer_data) &&
        !is_enabled_data_all_color_channels(clear_enabled_data);
    
    load_depthbuffer =
        is_framebuffer_data_depthbuffer_enabled(framebuffer_data) && 
        get_enabled_depth_data(clear_enabled_data) == 0;

    load_stencilbuffer =
        is_framebuffer_data_stencilbuffer_enabled(framebuffer_data) &&
        !is_enabled_data_all_stencil_bits(clear_enabled_data);

    #pragma unroll
    for (int pixel = get_local_id(0); pixel < CR_TILE_SQR; pixel += get_local_size(0)) {
        
        int2 surf = get_2d_surface_from_tile(tile, pixel);

        if (is_surface_out_viewport(surf, viewport)) continue;
        
        {
            uint color;

            if (load_colorbuffer) 
            {
                color = read_colorbuffer(colorbuffer, surf, viewport, colorbuffer_mode);
            }

            l1_color[pixel] = clear_color(color, clear_write_values, clear_enabled_data); 
        }

        {
            ushort depth;

            if (load_depthbuffer) 
            {
                depth = read_depthbuffer(depthbuffer, surf, viewport);
            }

            l1_depth[pixel] = clear_depth(depth, clear_write_values, clear_enabled_data);
        }

        {
            uchar stencil;

            if (load_stencilbuffer) 
            {
                stencil = read_stencilbuffer(stencilbuffer, surf, viewport);
            }

            l1_stencil[pixel] = clear_stencil(stencil, clear_write_values, clear_enabled_data);
        }
    }
}

// Write tile back to the framebuffer.
static inline void local_1dim_store_local_mem_to_framebuffer(
    colorbuffer_t colorbuffer, depthbuffer_t depthbuffer, stencilbuffer_t stencilbuffer,
    local uint* restrict l1_color, local ushort* restrict l1_depth, local uchar* restrict l1_stencil,
    const gl_framebuffer_data_t framebuffer_data,
    const uint colorbuffer_mode,
    const int2 tile, const uint2 viewport
)
{
    bool store_colorbuffer, store_depthbuffer, store_stencilbuffer;

    store_colorbuffer = is_framebuffer_data_colorbuffer_enabled(framebuffer_data);
    
    store_depthbuffer = is_framebuffer_data_depthbuffer_enabled(framebuffer_data);

    store_stencilbuffer = is_framebuffer_data_stencilbuffer_enabled(framebuffer_data);

    #pragma unroll
    for (int pixel = get_local_id(0); pixel < CR_TILE_SQR; pixel += get_local_size(0)) {
        
        int2 surf = get_2d_surface_from_tile(tile, pixel);

        if (is_surface_out_viewport(surf, viewport)) continue;

        if (store_colorbuffer) 
        {
            write_colorbuffer(colorbuffer, surf, viewport, colorbuffer_mode, l1_color[pixel]);
        }

        if (store_depthbuffer) 
        {
            write_depthbuffer(depthbuffer, surf, viewport, l1_depth[pixel]);
        }

        if (store_stencilbuffer) 
        {
            write_stencilbuffer(stencilbuffer, surf, viewport, l1_stencil[pixel]);
        }

    }
}

typedef union {
    uint                integer [DEVICE_SUB_GROUP_THREADS];
    sub_group_mask_t    mask    [DEVICE_SUB_GROUP_THREADS];
    sub_group_mask_t    tile    [CR_TILE_SQR];
} sg_tmp_mem_t;

static inline void local_1dim_execute_rop(
    local volatile uint*                restrict l1_color, 
    local volatile ushort*              restrict l1_depth, 
    local volatile uchar*               restrict l1_stencil,
    local volatile sg_tmp_mem_t*        restrict l_temp,
    const rop_config_t rop_config,
    const uint pixel_in_tile,
    const fragment_shader_output_t fs_output,
    const ushort pre_depth,
    bool active_rop_lane
) {
    sub_group_mask_t thread_bit = get_thread_bit_sub_group_mask();
    sub_group_mask_t lt_mask = get_lane_sub_group_mask_lt();
    sub_group_mask_t active_mask;

    local volatile sg_tmp_mem_t* l1_temp = &l_temp[get_sub_group_id()];

    do
    {
        local_1dim_barrier(CLK_LOCAL_MEM_FENCE);
        if (active_rop_lane) 
            clear_sub_group_mask(&l1_temp->tile[pixel_in_tile]);

        local_1dim_barrier(CLK_LOCAL_MEM_FENCE);

        if (active_rop_lane) 
            atomic_or_sub_group_mask(&l1_temp->tile[pixel_in_tile], thread_bit);

        local_1dim_barrier(CLK_LOCAL_MEM_FENCE);

        if (active_rop_lane) 
        {
            sub_group_mask_t thread_pixel_mask;

            #ifdef DEVICE_BARRIER_SYNC_LOCAL_ATOMIC
                thread_pixel_mask = l1_temp->tile[pixel_in_tile];
            #else
                thread_pixel_mask = atomic_or_sub_group_mask(&l1_temp->tile[pixel_in_tile], get_sub_group_mask_zero());
            #endif

            if (!any_sub_group_mask(and_sub_group_mask(thread_pixel_mask, lt_mask))) 
            {
                execute_ROP_single_sample(
                    fs_output, pre_depth,
                    &l1_color[pixel_in_tile], &l1_depth[pixel_in_tile], &l1_stencil[pixel_in_tile],
                    rop_config 
                );
                active_rop_lane = false;
            }
            
        }

        local_1dim_barrier(CLK_LOCAL_MEM_FENCE);

        active_mask = local_1dim_ballot(active_rop_lane, l_temp->mask);

    } while(any_sub_group_mask(active_mask));
}

/*
static inline void local_1dim_execute_rop(
    local volatile uint*                restrict l1_color, 
    local volatile ushort*              restrict l1_depth, 
    local volatile uchar*               restrict l1_stencil,
    local volatile sg_tmp_mem_t*        restrict l_temp,
    const rop_config_t rop_config,
    const uint pixel_in_tile,
    const fragment_shader_output_t fs_output,
    const ushort pre_depth,
    bool active_rop_lane
) {
    sub_group_mask_t thread_bit = get_thread_bit_sub_group_mask();
    sub_group_mask_t lt_mask = get_lane_sub_group_mask_lt();
    sub_group_mask_t empty_mask;
    clear_sub_group_mask(&empty_mask);

    uint my_turn, max_turns;

    local volatile sg_tmp_mem_t* l1_temp = l_temp + get_sub_group_id();

    // maybe optimize this clear
    if (active_rop_lane) clear_sub_group_mask(&l1_temp->tile[pixel_in_tile]);

    local_1dim_barrier(CLK_LOCAL_MEM_FENCE);

    if (active_rop_lane) atomic_or_sub_group_mask(&l1_temp->tile[pixel_in_tile], thread_bit);

    local_1dim_barrier(CLK_LOCAL_MEM_FENCE);
    
    sub_group_mask_t thread_pixel_mask = atomic_or_sub_group_mask(&l1_temp->tile[pixel_in_tile], empty_mask);

    my_turn = popcount_sub_group_mask(and_sub_group_mask(thread_pixel_mask, lt_mask));

    max_turns = local_1dim_reduce_max(my_turn, l_temp->integer);

    for(uint turn=0; turn < max_turns; ++turn)
    {
        if (my_turn == turn && active_rop_lane)
        {
            execute_ROP_single_sample(
                fs_output, pre_depth,
                &l1_color[pixel_in_tile], &l1_depth[pixel_in_tile], &l1_stencil[pixel_in_tile],
                rop_config 
            );
        }
        local_1dim_barrier(CLK_LOCAL_MEM_FENCE);
    }
}
*/


#define TRIANGLE_BUFFER_ELEMS (DEVICE_SUB_GROUP_THREADS * 2)

/**
 * Single-sample rasterization kernel.
 *
 * If device sub-group is enabled, multiple tiles are processed in parallel.
 * Otherwise, only one tile is processed per work group.
 */
kernel 
#ifdef DEVICE_SUB_GROUP_ENABLED
    __attribute__((reqd_work_group_size(DEVICE_SUB_GROUP_THREADS, DEVICE_FINE_SUB_GROUPS, 1)))
#else
    #error This kernel requires DEVICE_SUB_GROUP_ENABLED
#endif
void fine_raster_single_sample(
    global const void* gl_uniforms,
    FS_KERNEL_PARAMS

    global int* restrict a_fine_counter,
    global const int* restrict a_num_active_tiles,
    global const int* restrict a_num_bin_segs,
    global const int* restrict a_num_subtris,
    global const int* restrict a_num_tile_segs,
    
    global const int* restrict g_active_tiles,
    global const int* restrict g_tile_first_seg,
    global const int* restrict g_tile_seg_count,
    global const int* restrict g_tile_seg_data,
    global const int* restrict g_tile_seg_next,
    global const triangle_header_t* restrict g_tri_header,

    colorbuffer_t t_colorbuffer,
    depthbuffer_t t_depthbuffer,
    stencilbuffer_t t_stencilbuffer,
    #ifdef DEVICE_IMAGE_ENABLED
    read_only image1d_buffer_t t_tri_data,
    read_only image1d_buffer_t t_tri_header,
    #else
    global const triangle_data_t* restrict g_tri_data,
    #endif
    ro_vertex_buffer_t vertex_buffer,
    constant rop_config_t* restrict g_rop_config,

    const ulong  c_clear_write_values,
    const ushort c_clear_enabled_data,
    const uint   c_color_buffer_mode,
    const int    c_max_bin_segs,
    const int    c_max_subtris,
    const int    c_max_tile_segs,
    const int    c_viewport_height,
    const int    c_viewport_width,
    const int    c_width_tiles,
    const gl_framebuffer_data_t c_framebuffer_data
)
{
                                                                            // for 20 warps:
    local volatile ulong    s_cover8x8_lut      [CR_COVER8X8_LUT_SIZE];           // 6KB
    local volatile uint     s_tile_color        [DEVICE_FINE_SUB_GROUPS][CR_TILE_SQR]; // 5KB
    local volatile ushort   s_tile_depth        [DEVICE_FINE_SUB_GROUPS][CR_TILE_SQR]; // 2.5KB
    local volatile uchar    s_tile_stencil      [DEVICE_FINE_SUB_GROUPS][CR_TILE_SQR]; // 1.25KB
    local volatile uint     s_triangle_idx      [DEVICE_FINE_SUB_GROUPS][TRIANGLE_BUFFER_ELEMS];          // 5KB  original triangle index
    local volatile uint     s_tri_data_idx      [DEVICE_FINE_SUB_GROUPS][TRIANGLE_BUFFER_ELEMS];          // 5KB  CRTriangleData index
    local volatile ulong    s_triangle_cov      [DEVICE_FINE_SUB_GROUPS][TRIANGLE_BUFFER_ELEMS];          // 10KB coverage mask
    local volatile uint     s_triangle_frag     [DEVICE_FINE_SUB_GROUPS][TRIANGLE_BUFFER_ELEMS];          // 5KB  fragment index
    local volatile uchar    l_triangle_conf    [DEVICE_FINE_SUB_GROUPS][TRIANGLE_BUFFER_ELEMS];          // 0.25KB  per-triangle configuration

    // The required local mem for specific sub-group communications.

    local volatile sg_tmp_mem_t     l_temp              [DEVICE_FINE_SUB_GROUPS]; // tmp memory
                                                                            // = 47.25KB total
    // Warp Space
    local volatile uint*    w_tile_color        = (local volatile uint*)    &s_tile_color[get_local_id(1)];
    local volatile ushort*  w_tile_depth        = (local volatile ushort*)  &s_tile_depth[get_local_id(1)];
    local volatile uchar*   w_tile_stencil      = (local volatile uchar*)   &s_tile_stencil[get_local_id(1)];
    local volatile uint*    w_triangle_idx      = (local volatile uint*)    &s_triangle_idx[get_local_id(1)];
    local volatile uint*    w_tri_data_idx      = (local volatile uint*)    &s_tri_data_idx[get_local_id(1)];
    local volatile ulong*   w_triangle_cov      = (local volatile ulong*)   &s_triangle_cov[get_local_id(1)];
    local volatile uint*    w_triangle_frag     = (local volatile uint*)    &s_triangle_frag[get_local_id(1)];
    local volatile uchar*    sg_triangle_conf   = (local volatile uchar*)    &l_triangle_conf[get_local_id(1)];

    local volatile sg_tmp_mem_t *sg_temp = &l_temp[get_sub_group_id()];

    if (*a_num_subtris > c_max_subtris || *a_num_bin_segs > c_max_bin_segs || *a_num_tile_segs > c_max_tile_segs)
        return;

    sub_group_mask_t rop_lane_mask = determine_ROP_lane_mask(/*c_render_mode_flags*/ 0); // TODO: this may important for unordered primitives rendering 
    cover8x8_setupLUT(s_cover8x8_lut);
    barrier(CLK_LOCAL_MEM_FENCE);

    // common logic
    bool colorbuffer_clear = 
        (c_clear_enabled_data & CLEAR_ENABLED_COLOR_CHANNEL_MASK) != 0;
    bool depthbuffer_clear = 
        (c_clear_enabled_data & CLEAR_ENABLED_DEPTH_CHANNEL) != 0;
    bool stencilbuffer_clear = 
        (c_clear_enabled_data & CLEAR_ENABLED_STENCIL_CHANNEL_MASK) != 0;

    bool colorbuffer_full_clear = 
        (c_clear_enabled_data & ENABLED_COLOR_CHANNEL_MASK) == ENABLED_COLOR_CHANNEL_MASK;
    bool stencilbuffer_full_clear = 
        (c_clear_enabled_data & CLEAR_ENABLED_STENCIL_CHANNEL_MASK) == CLEAR_ENABLED_STENCIL_CHANNEL_MASK;

    // bool depth_test_enabled = (c_render_mode_flags & RENDER_MODE_FLAG_ENABLE_DEPTH) != 0;
    // bool stencil_test_enabled = (c_render_mode_flags & RENDER_MODE_FLAG_ENABLE_STENCIL) != 0;

    // loop over tiles
    for (;;)
    {
        // each warp pick a tile
        int active_idx;
        if (get_local_id(0) == 0)
            active_idx = atomic_add(a_fine_counter, 1);

        active_idx = local_1dim_broadcast(active_idx, 0, l_temp->integer);
        
        if (active_idx >= *a_num_active_tiles)
            break;

        int tile_idx = g_active_tiles[active_idx];
        int segment = g_tile_first_seg[tile_idx];
        int tile_y = idiv_fast(tile_idx, c_width_tiles);
        int tile_x = tile_idx - tile_y * c_width_tiles;

        // initialize per-tile state
        int tri_read = 0, tri_write = 0;
        int frag_read = 0, frag_write = 0;
        w_triangle_frag[TRIANGLE_BUFFER_ELEMS-1] = 0; // "previous triangle"

        // load tile
        local_1dim_load_and_clean_framebuffer_to_local_mem(
            t_colorbuffer, t_depthbuffer, t_stencilbuffer,
            (local uint*) w_tile_color, (local ushort*) w_tile_depth, (local uchar*) w_tile_stencil,
            c_framebuffer_data,
            c_clear_write_values, (enabled_data_t) {c_clear_enabled_data},
            c_color_buffer_mode,
            (int2) {tile_x, tile_y}, (uint2) {c_viewport_width, c_viewport_height}
        );
        

        // bound tile z
        ushort tile_z_max, tile_z_min;
        bool tile_z_upd_max, tile_z_upd_min;
        init_tile_z_max(&tile_z_max, &tile_z_upd_max, w_tile_depth);
        init_tile_z_min(&tile_z_min, &tile_z_upd_min, w_tile_depth);

        // process fragments in tile
        for(;;)
        {
            // TODO: enqueue fragments that share same configuration
            // need to queue more fragments?
            if (frag_write - frag_read < get_local_size(0) && segment >= 0)
            {
                // update tile z

                // TODO: Optimize for non ordering req primitives.
                // #ifdef DEVICE_SUB_GROUP_ENABLED
                // sub_group_update_tile_z_max(c_render_mode_flags, &tile_z_max, &tile_z_upd_max, w_tile_depth);
                // sub_group_update_tile_z_min(c_render_mode_flags, &tile_z_min, &tile_z_upd_min, w_tile_depth);
                // #else
                // local_1dim_update_tile_z_max(c_render_mode_flags, &tile_z_max, &tile_z_upd_max, w_tile_depth, s_temp);
                // local_1dim_update_tile_z_min(c_render_mode_flags, &tile_z_min, &tile_z_upd_min, w_tile_depth, s_temp);
                // #endif
                
                // read triangles
                do
                {
                    // read triangle index and data, advance to next segment
                    int tri_idx, data_idx;
                    uint4 tri_header;
     
                    {
                        get_triangle(&tri_idx, &data_idx, &tri_header, &segment, 
                            g_tri_header, g_tile_seg_data, g_tile_seg_next, g_tile_seg_count
                            #ifdef DEVICE_IMAGE_ENABLED
                            , t_tri_header
                            #endif
                            );

                        // early z cull
                        // if stencil is enabled or fragment can be discarded
                        //if (tri_idx >= 0 && early_z_cull(c_render_mode_flags, tri_header, tile_z_min, tile_z_max, c_depth_data))
                        //    tri_idx = -1;
                        
                    }

                    // determine coverage
                    ulong coverage = triangle_pixel_coverage(0, tri_header, tile_x, tile_y, s_cover8x8_lut, c_viewport_width, c_viewport_height);
                    int pop = (tri_idx == -1) ? 0 : popcount(coverage);

                    // fragment count scan
                    uint frag = local_1dim_scan_inclusive_add(pop, l_temp->integer);
                    uint tmp_frag = frag;
                    frag += frag_write; // frag now holds cumulative fragment count
                    frag_write += local_1dim_broadcast(tmp_frag, get_sub_group_size() - 1, l_temp->integer);

                    // queue non-empty triangles
                    sub_group_mask_t good_mask = local_1dim_ballot(pop != 0, l_temp->mask);

                    if (pop != 0)
                    {
                        sub_group_mask_t lt_mask = get_lane_sub_group_mask_lt();
                        int idx = popcount_sub_group_mask(and_sub_group_mask(good_mask, lt_mask));
                        idx = (tri_write + idx) % TRIANGLE_BUFFER_ELEMS; // wrap index
                        w_triangle_idx  [idx] = tri_idx;
                        w_tri_data_idx  [idx] = data_idx;
                        w_triangle_frag [idx] = frag;
                        w_triangle_cov  [idx] = coverage;
                        
                        triangle_header_t th = *((triangle_header_t*) &tri_header);
                        sg_triangle_conf[idx] = get_th_misc_primitive_config(th.misc);
                    }
                    tri_write += popcount_sub_group_mask(good_mask);

                }
                while (frag_write - frag_read < get_sub_group_size() && segment >= 0);
            }
            
            // end of segment?
            if (frag_read == frag_write)
                break;
            
            // TODO: refactor this section
            // tag triangle boundaries

            sg_temp->integer[get_sub_group_local_id()] = 0;
            local_1dim_barrier(CLK_LOCAL_MEM_FENCE);

            if (tri_read + get_sub_group_local_id() < tri_write)
            {
                int idx = w_triangle_frag[(tri_read + get_sub_group_local_id()) % TRIANGLE_BUFFER_ELEMS] - frag_read;
                if (idx <= get_sub_group_size())
                    sg_temp->integer[idx - 1] = 1;
            }
            local_1dim_barrier(CLK_LOCAL_MEM_FENCE);

            // distribute fragments
            int rop_lane_idx = popcount_sub_group_mask(rop_lane_mask);
            bool tagged = sg_temp->integer[rop_lane_idx];
            local_1dim_barrier(CLK_LOCAL_MEM_FENCE);
            sub_group_mask_t boundary_mask = local_1dim_ballot(tagged, l_temp->mask);

            /* TODO: Check why does not work
            local_1dim_barrier(CLK_LOCAL_MEM_FENCE);
            int rop_lane_idx = popcount_sub_group_mask(rop_lane_mask);
            if (tri_read + get_sub_group_local_id() < tri_write)
            {
                int idx = w_triangle_frag[(tri_read + get_sub_group_local_id()) % TRIANGLE_BUFFER_ELEMS] - frag_read;
                
                if (idx <= get_sub_group_size()) {
                    sub_group_mask_t mask;
                    clear_sub_group_mask(&mask);
                    set_bit_sub_group_mask(&mask, idx-1);
                    atomic_or_sub_group_mask(sg_temp->mask, mask);
                }
            }
            local_1dim_barrier(CLK_LOCAL_MEM_FENCE);
            sub_group_mask_t boundary_mask = atomic_or_sub_group_mask(sg_temp->mask, (sub_group_mask_t){0L});
            local_1dim_barrier(CLK_LOCAL_MEM_FENCE);
            */


            bool active_rop_lane = rop_lane_idx < frag_write - frag_read; 
            int pixel_in_tile = 0;
            ushort depth;
            uchar conf_idx;
            fragment_shader_output_t fragment_shader_output;
            rop_config_t rop_config;

            if (active_rop_lane)
            {
                int tri_buf_idx = (tri_read + popcount_sub_group_mask(and_sub_group_mask(boundary_mask,rop_lane_mask))) % TRIANGLE_BUFFER_ELEMS;
                int frag_idx = add_sub(frag_read, rop_lane_idx, w_triangle_frag[(tri_buf_idx - 1 + TRIANGLE_BUFFER_ELEMS) % TRIANGLE_BUFFER_ELEMS]);
                ulong coverage = w_triangle_cov[tri_buf_idx];
                pixel_in_tile = find_fragment(coverage, frag_idx);
                int tri_idx = w_triangle_idx[tri_buf_idx];
                int data_idx = w_tri_data_idx[tri_buf_idx];

                // determine pixel position
                uint pixel_x = (tile_x << CR_TILE_LOG2) + (pixel_in_tile & 7);
                uint pixel_y = (tile_y << CR_TILE_LOG2) + (pixel_in_tile >> 3);

                // TODO: Optimize to avoid running ROP
                // pre ROP tests
                conf_idx = sg_triangle_conf[tri_buf_idx];
                rop_config = g_rop_config[conf_idx];
                // stencil test
                uchar stencil;
                bool skill = false;
                
                // depth test
                bool zkill = false;
                
                if (!skill && (rop_config.render_mode.flags & RENDER_MODE_FLAG_ENABLE_DEPTH) != 0)
                {
                
                    uint4 zdata;
                    #ifdef DEVICE_IMAGE_ENABLED
                    zdata = read_imageui(t_tri_data, data_idx * 4);
                    #else
                    zdata = *((global uint4*) &g_tri_data[data_idx]);
                    #endif
                    depth = (zdata.x * pixel_x + zdata.y * pixel_y + zdata.z) >> 16;
                    ushort old_depth = w_tile_depth[pixel_in_tile];
                    if (!depth_test(depth, old_depth, rop_config.depth_data))
                        zkill = true;
                    else {
                        // TODO: Checkout this
                        tile_z_upd_max = update_tile_z(old_depth, tile_z_max, rop_config.depth_data); // we are replacing previous zmax => need to update
                        tile_z_upd_min = update_tile_z(old_depth, tile_z_min, rop_config.depth_data); // we are replacing previous zmax => need to update
                    }
                }

                // fragment_shader_output.discard = false;

                if (!skill && !zkill)
                {
                    // run fragment shader
                    
                    active_rop_lane = run_fragment_shader(
                        (global void*)((global uchar*)gl_uniforms + DEVICE_UNIFORM_CAPACITY*conf_idx),
                        FS_KERNEL_ARGS

                        &fragment_shader_output,
                        data_idx, pixel_x, pixel_y,
                        #ifdef DEVICE_IMAGE_ENABLED
                        t_tri_data,
                        #else
                        g_tri_data,
                        #endif
                        vertex_buffer
                        );
                        
                }

            }

            // TODO: Optimize, ordering on primitive is not always required.
            // loop while multiple threads access to same pixel and run ROP
            
            local_1dim_execute_rop(
                w_tile_color, w_tile_depth, w_tile_stencil, l_temp,
                rop_config, pixel_in_tile, fragment_shader_output, depth,
                active_rop_lane
            );

            // if (active_rop_lane) w_tile_color[pixel_in_tile] = 0xFFFF00FFu; DEBUG
            // update counters
            frag_read = min((int)(frag_read + get_local_size(0)), frag_write);
            tri_read += popcount_sub_group_mask(boundary_mask);
        }

        local_1dim_store_local_mem_to_framebuffer(
            t_colorbuffer, t_depthbuffer, t_stencilbuffer,
            (local uint*) w_tile_color, (local ushort*) w_tile_depth, (local uchar*) w_tile_stencil,
            c_framebuffer_data,
            c_color_buffer_mode,
            (int2) {tile_x, tile_y}, (uint2) {c_viewport_width, c_viewport_height}
        );
    }

}
