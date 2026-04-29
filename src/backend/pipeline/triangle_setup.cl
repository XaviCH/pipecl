#ifdef __COMPILER_RELATIVE_PATH__
    #include <backend/types.cl>
    #include <backend/utils/common.cl>
    #include <backend/extensions/cl_khr_global_int32_base_atomics/include.cl>
    #include <backend/extensions/cl_khr_local_int32_base_atomics/include.cl>
    #include <backend/extensions/cl_khr_local_int32_extended_atomics/include.cl>
#else
#include "glsc2/src/backend/types.cl"
#include "glsc2/src/backend/utils/common.cl"
#endif

/**
 * @brief 
 * 
 * @param a_num_subtris num of triangles + num of subtris generated
 * @param g_tri_header 
 * @param g_tri_data 
 * @param g_tri_subtris num of subtris related with triangle idx
 *
 *
 * @todo benchmark local enqueueing for subtriangles and global flush at the end.
 */
inline void triangle_setup(
    global int* a_num_subtris,

    global triangle_header_t* g_tri_header, 
    global triangle_data_t* g_tri_data,
    global uchar* g_tri_subtris,

    ro_vertex_buffer_t t_vertex_buffer, 

    const int c_max_subtris,
    const render_mode_t c_render_mode,
    const int c_samples_log2,
    const int c_vertex_size,
    const int c_viewport_height,
    const int c_viewport_width,
    int3 vidx,
    float* bary,
    const uint c_primitive_config
);

//------------------------------------------------------------------------

/**
 *  Arrays render mode
 *
 */
kernel
//__attribute__((reqd_work_group_size(DEVICE_SUB_GROUP_THREADS, DEVICE_SETUP_SUB_GROUPS, 1)))
__attribute__((reqd_work_group_size(DEVICE_SETUP_THREADS, 1, 1)))
void triangle_setup_arrays(
    global int* a_num_subtris,

    global triangle_header_t* g_tri_header, 
    global triangle_data_t* g_tri_data,
    global uchar* g_tri_subtris,

    ro_vertex_buffer_t t_vertex_buffer, 

    const int c_num_tris,
    const int c_vertex_offset,
    const int c_max_subtris,
    const render_mode_t c_render_mode,
    const int c_samples_log2,
    const int c_vertex_size,
    const int c_viewport_height,
    const int c_viewport_width,
    const uint c_primitive_config
)
{
    // TODO: Check register ocupancy to decide if use shared or register
    /*
    local float s_bary[DEVICE_SETUP_SUB_GROUPS * DEVICE_SUB_GROUP_THREADS][18];

    local float* bary = s_bary[get_local_linear_id()];
    */
    float bary[18];

    // Pick a task.

    // int task_idx = get_global_id(0);
    int task_idx = get_global_linear_id();
    
    if (task_idx >= c_num_tris)
        return;
    
    // Pick vertices

    int3 vidx;
    if (is_render_mode_flag_triangle_fan(c_render_mode)) {
        vidx = (int3){0, task_idx + 1, task_idx + 2};
    } else if (is_render_mode_flag_triangle_strip(c_render_mode)) {
        uint offset = 2 * (task_idx%2);
        vidx = (int3){task_idx + offset, task_idx + 1, task_idx + 2 - offset};
    } else {
        vidx = (int3){task_idx*3 + 0, task_idx*3 + 1, task_idx*3 + 2};
    }

    vidx += c_vertex_offset;

    // Read vertices.

    triangle_setup(
        a_num_subtris,
        g_tri_header, 
        g_tri_data,
        g_tri_subtris,
        t_vertex_buffer,
        c_max_subtris,
        c_render_mode,
        c_samples_log2,
        c_vertex_size,
        c_viewport_height,
        c_viewport_width,
        vidx,
        bary,
        c_primitive_config
    );
}

/**
 *  Range render mode
 *
 */
kernel
// __attribute__((reqd_work_group_size(DEVICE_SUB_GROUP_THREADS, DEVICE_SETUP_SUB_GROUPS, 1)))
__attribute__((reqd_work_group_size(DEVICE_SETUP_THREADS, 1, 1)))
void triangle_setup_range(
    global int* a_num_subtris,

    global const ushort* g_index_buffer,
    global triangle_header_t* g_tri_header, 
    global triangle_data_t* g_tri_data,
    global uchar* g_tri_subtris,

    ro_vertex_buffer_t t_vertex_buffer,

    const int c_num_tris,
    const int c_vertex_offset,
    const int c_max_subtris,
    const render_mode_t c_render_mode,
    const int c_samples_log2,
    const int c_vertex_size,
    const int c_viewport_height,
    const int c_viewport_width,
    const uint c_primitive_config
)
{
    /*
    local float s_bary[DEVICE_SETUP_SUB_GROUPS * DEVICE_SUB_GROUP_THREADS][18];

    local float* bary = s_bary[get_local_linear_id()];
    */
    float bary[18];

    // Pick a task.

    int task_idx = get_global_linear_id();
    
    if (task_idx >= c_num_tris)
        return;
    
    // Pick vertices

    int3 vidx;
    if (is_render_mode_flag_triangle_fan(c_render_mode)) {
        vidx = (int3){
            g_index_buffer[0], 
            g_index_buffer[task_idx + 1], 
            g_index_buffer[task_idx + 2]
        };
    } else if (is_render_mode_flag_triangle_strip(c_render_mode)) {
        uint offset = 2 * (task_idx%2);
        vidx = (int3){
            g_index_buffer[task_idx + offset], 
            g_index_buffer[task_idx + 1], 
            g_index_buffer[task_idx + 2 - offset]
        };
    } else {
        vidx = (int3){
            g_index_buffer[task_idx*3 + 0], 
            g_index_buffer[task_idx*3 + 1], 
            g_index_buffer[task_idx*3 + 2]
        };
    }

    vidx += c_vertex_offset;

    // Read vertices.

    triangle_setup(
        a_num_subtris,
        g_tri_header, 
        g_tri_data,
        g_tri_subtris,
        t_vertex_buffer,
        c_max_subtris,
        c_render_mode,
        c_samples_log2,
        c_vertex_size,
        c_viewport_height,
        c_viewport_width,
        vidx,
        bary,
        c_primitive_config
    );
}


// ---------------------------------------

/**
    Calculate window positions of points and limit box
 */
static inline void snap_triangle(
    float4 v0, float4 v1, float4 v2,
    int2* p0, int2* p1, int2* p2, float3* rcpW, int2* lo, int2* hi,
    int c_viewport_width, int c_viewport_height 
    )
{
    float viewScaleX = (float)(c_viewport_width  << (CR_SUBPIXEL_LOG2 - 1));
    float viewScaleY = (float)(c_viewport_height << (CR_SUBPIXEL_LOG2 - 1));
    *rcpW = (float3)(1.0f / v0.w, 1.0f / v1.w, 1.0f / v2.w);
    *p0 = (int2)(convert_int_sat(round(v0.x * rcpW->x * viewScaleX)), convert_int_sat(round(v0.y * rcpW->x * viewScaleY)));
    *p1 = (int2)(convert_int_sat(round(v1.x * rcpW->y * viewScaleX)), convert_int_sat(round(v1.y * rcpW->y * viewScaleY)));
    *p2 = (int2)(convert_int_sat(round(v2.x * rcpW->z * viewScaleX)), convert_int_sat(round(v2.y * rcpW->z * viewScaleY)));
    *lo = (int2)(min(p0->x, min(p1->x, p2->x)), min(p0->y, min(p1->y, p2->y)));
    *hi = (int2)(max(p0->x, max(p1->x, p2->x)), max(p0->y, max(p1->y, p2->y)));
}

/**
    Cull checking
 */
inline int prepareTriangle(
    int2 p0, int2 p1, int2 p2, int2 lo, int2 hi,
    int2* d1, int2* d2, int* area, 
    int c_viewport_width, int c_viewport_height,
    int c_samples_log2
) {
    // Backfacing or degenerate => cull.

    *d1 = p1 - p0;
    *d2 = p2 - p0;
    *area = d1->x * d2->y - d1->y * d2->x;

    if (*area <= 0)
        return 2; // Backfacing.

    // AABB falls between samples => cull.

    int sampleSize = 1 << (CR_SUBPIXEL_LOG2 - c_samples_log2);
    int2 bias = (int2)(c_viewport_width, c_viewport_height) << (CR_SUBPIXEL_LOG2 - 1) - (sampleSize >> 1);
    int2 low = (lo + bias + (sampleSize - 1)) & -sampleSize;
    int2 high = (hi + bias) & -sampleSize;

    if (low.x > high.x || low.y > high.y)
        return 3; // Between pixels.

    // AABB covers 1 or 2 samples => cull if they are not covered.

    int diff = high.x + high.y - low.x - low.y;
    if (diff <= sampleSize)
    {
        int2 t0 = p0 + bias - low;
        int2 t1 = p1 + bias - low;
        int2 t2 = p2 + bias - low;

        int e0 = t0.x * t1.y - t0.y * t1.x;
        int e1 = t1.x * t2.y - t1.y * t2.x;
        int e2 = t2.x * t0.y - t2.y * t0.x;

        if (e0 < 0 || e1 < 0 || e2 < 0)
        {
            if (diff == 0)
                return 4; // Between pixels.

            t0 = p0 + bias - high;
            t1 = p1 + bias - high;
            t2 = p2 + bias - high;

            e0 = t0.x * t1.y - t0.y * t1.x;
            e1 = t1.x * t2.y - t1.y * t2.x;
            e2 = t2.x * t0.y - t2.y * t0.x;

            if (e0 < 0 || e1 < 0 || e2 < 0)
                return 5; // Between pixels.
        }
    }

    // Otherwise => proceed to output the triangle.

    return 0; // Visible.
}

//------------------------------------------------------------------------

inline void setupTriangle(
    global triangle_header_t* restrict th, global triangle_data_t* restrict td, int3 vidx,
    float4 v0, float4 v1, float4 v2,
    float2 b0, float2 b1, float2 b2,
    int2 p0, int2 p1, int2 p2, float3 rcpW,
    int2 d1, int2 d2, int area,

    int c_viewport_width, int c_viewport_height,
    int c_samples_log2, render_mode_t c_render_mode,
    face_t face, uint c_primitive_config
    )
{
    uint dep = 0;
    float areaRcp;
    int2 wv0;

    if (is_render_mode_flag_enable_depth(c_render_mode) ||
        is_render_mode_flag_enable_lerp(c_render_mode))
    {
        areaRcp = 1.0f / (float)area;
        wv0.x = p0.x + (c_viewport_width  << (CR_SUBPIXEL_LOG2 - 1));
        wv0.y = p0.y + (c_viewport_height << (CR_SUBPIXEL_LOG2 - 1));
    }

    // Setup depth plane equation.

    uint3 zpleq;
    uint zmin = 0, zslope = 0;
    if (is_render_mode_flag_enable_depth(c_render_mode))
    {
        float zcoef = (float)(CR_DEPTH_MAX - CR_DEPTH_MIN) * 0.5f;
        float zbias = (float)(CR_DEPTH_MAX + CR_DEPTH_MIN) * 0.5f;
        float3 zvert;
        zvert.x = (v0.z * zcoef) * rcpW.x + zbias;
        zvert.y = (v1.z * zcoef) * rcpW.y + zbias;
        zvert.z = (v2.z * zcoef) * rcpW.z + zbias;

        int2 zv0;
        zv0 = wv0 - (1 << (CR_SUBPIXEL_LOG2 - c_samples_log2 - 1));
        zpleq = setupPleq(zvert, zv0, d1, d2, areaRcp, c_samples_log2);

        zmin = convert_uint_sat(fmin(fmin(zvert.x, zvert.y), zvert.z) - (float)CR_LERP_ERROR(c_samples_log2));
        if (c_samples_log2 != 0)
        {
            uint tmp = abs((int)zpleq.x) + abs(max((int)zpleq.y, -INT_MAX));
            zslope = tmp << max(c_samples_log2 - 1, 0);
            if ((zslope >> max(c_samples_log2 - 1, 0)) != tmp)
                zslope = UINT_MAX;
        }

        dep += zpleq.x + zpleq.y + zpleq.z + zmin + zslope;
    }
    
    // Setup lerp plane equations.
    
    uint3 wpleq, upleq, vpleq;
    float3 wvert, uvert, vvert;
    if (is_render_mode_flag_enable_lerp(c_render_mode))
    {
        float wcoef = fmin(fmin(v0.w, v1.w), v2.w) * (float)CR_BARY_MAX;
        wvert = (float3)(wcoef * rcpW.x, wcoef * rcpW.y, wcoef * rcpW.z);
        uvert = (float3)(b0.x * wvert.x, b1.x * wvert.y, b2.x * wvert.z);
        vvert = (float3)(b0.y * wvert.x, b1.y * wvert.y, b2.y * wvert.z);

        wpleq = setupPleq(wvert, wv0, d1, d2, areaRcp, c_samples_log2 + 1);
        upleq = setupPleq(uvert, wv0, d1, d2, areaRcp, c_samples_log2 + 1);
        vpleq = setupPleq(vvert, wv0, d1, d2, areaRcp, c_samples_log2 + 1);
        dep += wpleq.x + wpleq.y + wpleq.z + upleq.x + upleq.y + upleq.z;
    }
    
    // Write triangle_data_t.

    if (is_render_mode_flag_enable_depth(c_render_mode)) {
        *(global uint4*)&td->zx = (uint4)(zpleq.x, zpleq.y, zpleq.z, zslope);
    }
    if (is_render_mode_flag_enable_lerp(c_render_mode)) {
        *(global uint4*)&td->wx = (uint4)(wpleq.x, wpleq.y, wpleq.z, upleq.x);
        *(global uint4*)&td->uy = (uint4)(upleq.y, upleq.z, vpleq.x, vpleq.y);
        *(global uint4*)&td->vb = (uint4)(vpleq.z, vidx.x, vidx.y, vidx.z);
    }
    else
    {
        *(global uint4*)&td->vb = (uint4)(0, vidx.x, vidx.y, vidx.z);
    }

    // Determine flipbits.

    uint f01 = cover8x8_selectFlips(d1.x, d1.y);
    uint f12 = cover8x8_selectFlips(d2.x - d1.x, d2.y - d1.y);
    uint f20 = cover8x8_selectFlips(-d2.x, -d2.y);

    // Write triangle_header_t.
    *(global uint4*)th = (uint4)(
        prmt(p0.x, p0.y, 0x5410),
        prmt(p1.x, p1.y, 0x5410),
        prmt(p2.x, p2.y, 0x5410),
        (zmin & 0xfffff000u) | (f01 << 6) | (f12 << 2) | (f20 >> 2));

    triangle_header_misc_t th_misc = th->misc; 
    set_th_misc_face(&th_misc, face);
    set_th_misc_primitive_config(&th_misc, c_primitive_config);
    th->misc = th_misc;
}

//------------------------------------------------------------------------

/**
    Depending on the mode, flip the triangle.
    Swap v0 and v2.
 */
static inline face_t flipTriangle(int3* vidx, float4* v0, float4* v1, float4* v2, const render_mode_t c_render_mode) 
{
    float2 v0w = v0->xy / v0->w;
    float2 v1w = v1->xy / v1->w;
    float2 v2w = v2->xy / v2->w;
    float2 d1 = v1w - v0w;
    float2 d2 = v2w - v0w;

    float area = d1.x * d2.y - d1.y * d2.x;

    bool need_to_flip =
        (area < 0 && !is_render_mode_flag_enable_cull_back(c_render_mode)) ||
        (area > 0 && is_render_mode_flag_enable_cull_front(c_render_mode)) ;

    if (need_to_flip) {
        int tidx = vidx->x;
        vidx->x = vidx->z;
        vidx->z = tidx;

        float4 tv = *v0;
        *v0 = *v2;
        *v2 = tv;
    }

    return area >= 0 ? FRONT : BACK;
}

inline void triangle_setup(
    global int* a_num_subtris,

    global triangle_header_t* g_tri_header, 
    global triangle_data_t* g_tri_data,
    global uchar* g_tri_subtris,

    ro_vertex_buffer_t t_vertex_buffer, 

    const int c_max_subtris,
    const render_mode_t c_render_mode,
    const int c_samples_log2,
    const int c_vertex_size,
    const int c_viewport_height,
    const int c_viewport_width,
    int3 vidx,
    float* bary,
    const uint c_primitive_config
)
{

    int2 p0, p1, p2, lo, hi, d1, d2;
    float3 rcpW;
    int area;

    // Pick a task.

    int task_idx = get_global_id(0);

    // Read vertices.

    int stride = c_vertex_size / sizeof(float4);

    float4 v0, v1, v2;
    v0 = read_vertex_buffer(t_vertex_buffer, vidx.x * stride);
    v1 = read_vertex_buffer(t_vertex_buffer, vidx.y * stride);
    v2 = read_vertex_buffer(t_vertex_buffer, vidx.z * stride);

    // Outside view frustum => cull.

    if (v0.w < fabs(v0.x) || v0.w < fabs(v0.y) || v0.w < fabs(v0.z))
    {
        if ((v0.w < +v0.x && v1.w < +v1.x && v2.w < +v2.x) |
            (v0.w < -v0.x && v1.w < -v1.x && v2.w < -v2.x) |
            (v0.w < +v0.y && v1.w < +v1.y && v2.w < +v2.y) |
            (v0.w < -v0.y && v1.w < -v1.y && v2.w < -v2.y) |
            (v0.w < +v0.z && v1.w < +v1.z && v2.w < +v2.z) |
            (v0.w < -v0.z && v1.w < -v1.z && v2.w < -v2.z))
        {
            g_tri_subtris[task_idx] = 0; 
            return;
        }
    }

    // Flip triangle depending on culling mode
    face_t face = flipTriangle(&vidx, &v0, &v1, &v2, c_render_mode);

    // Inside depth range => try to snap vertices.
    if (v0.w >= fabs(v0.z) && v1.w >= fabs(v1.z) && v2.w >= fabs(v2.z))
    {
        // Inside S16 range and small enough => fast path.
        // Note: aabbLimit comes from the fact that cover8x8
        // does not support guardband with maximal viewport.
        
        snap_triangle(v0, v1, v2, &p0, &p1, &p2, &rcpW, &lo, &hi, 
            c_viewport_width, c_viewport_height);
        int loxy = min(lo.x, lo.y);
        int hixy = max(hi.x, hi.y);
        int aabbLimit = (1 << (CR_MAXVIEWPORT_LOG2 + CR_SUBPIXEL_LOG2)) - 1;

        if (loxy >= -32768 && hixy <= 32767 && hixy - loxy <= aabbLimit)
        {
            int res = prepareTriangle(p0, p1, p2, lo, hi, &d1, &d2, &area, c_viewport_width, c_viewport_height, c_samples_log2);
            g_tri_subtris[task_idx] = (res == 0) ? 1 : 0;

            if (res == 0)
                setupTriangle(
                    &g_tri_header[task_idx], &g_tri_data[task_idx], vidx,
                    v0, v1, v2,
                    (float2)(0.0f, 0.0f),
                    (float2)(1.0f, 0.0f),
                    (float2)(0.0f, 1.0f),
                    p0, p1, p2, rcpW,
                    d1, d2, area,
                    c_viewport_width, c_viewport_height,
                    c_samples_log2, c_render_mode, face, c_primitive_config);

            return;
        }
    }

    // Clip to view frustum.

    float4 ov0 = v0;
    float4 od1 = (float4)(v1.x - v0.x, v1.y - v0.y, v1.z - v0.z, v1.w - v0.w);
    float4 od2 = (float4)(v2.x - v0.x, v2.y - v0.y, v2.z - v0.z, v2.w - v0.w);
    int numVerts = clipTriangleWithFrustum(bary, (float*) &ov0, (float*) &v1, (float*) &v2, (float*) &od1, (float*) &od2);

    v0.x = ov0.x + od1.x * bary[0] + od2.x * bary[1];
    v0.y = ov0.y + od1.y * bary[0] + od2.y * bary[1];
    v0.z = ov0.z + od1.z * bary[0] + od2.z * bary[1];
    v0.w = ov0.w + od1.w * bary[0] + od2.w * bary[1];
    v1.x = ov0.x + od1.x * bary[2] + od2.x * bary[3];
    v1.y = ov0.y + od1.y * bary[2] + od2.y * bary[3];
    v1.z = ov0.z + od1.z * bary[2] + od2.z * bary[3];
    v1.w = ov0.w + od1.w * bary[2] + od2.w * bary[3];
    float4 tv1 = v1;

    int numSubtris = 0;
    for (int i = 2; i < numVerts; i++)
    {
        v2.x = ov0.x + od1.x * bary[i * 2 + 0] + od2.x * bary[i * 2 + 1];
        v2.y = ov0.y + od1.y * bary[i * 2 + 0] + od2.y * bary[i * 2 + 1];
        v2.z = ov0.z + od1.z * bary[i * 2 + 0] + od2.z * bary[i * 2 + 1];
        v2.w = ov0.w + od1.w * bary[i * 2 + 0] + od2.w * bary[i * 2 + 1];

        snap_triangle(v0, v1, v2, &p0, &p1, &p2, &rcpW, &lo, &hi, c_viewport_width, c_viewport_height);
        if (prepareTriangle(p0, p1, p2, lo, hi, &d1, &d2, &area, c_viewport_width, c_viewport_height, c_samples_log2) == 0)
            numSubtris++;

        v1 = v2;
    }

    g_tri_subtris[task_idx] = numSubtris;

    // Multiple subtriangles => allocate.

    int subtriBase = task_idx;
    if (numSubtris > 1)
    {
        subtriBase = atomic_add(a_num_subtris, numSubtris);
        g_tri_header[task_idx].misc.misc = subtriBase;
        if (subtriBase + numSubtris > c_max_subtris)
            numVerts = 0;
    }

    // Setup subtriangles.

    v1 = tv1;
    for (int i = 2; i < numVerts; i++)
    {
        v2.x = ov0.x + od1.x * bary[i * 2 + 0] + od2.x * bary[i * 2 + 1];
        v2.y = ov0.y + od1.y * bary[i * 2 + 0] + od2.y * bary[i * 2 + 1];
        v2.z = ov0.z + od1.z * bary[i * 2 + 0] + od2.z * bary[i * 2 + 1];
        v2.w = ov0.w + od1.w * bary[i * 2 + 0] + od2.w * bary[i * 2 + 1];

        snap_triangle(v0, v1, v2, &p0, &p1, &p2, &rcpW, &lo, &hi, c_viewport_width, c_viewport_height);
        if (prepareTriangle(p0, p1, p2, lo, hi, &d1, &d2, &area, c_viewport_width, c_viewport_height, c_samples_log2) == 0)
        {

            setupTriangle(
                &g_tri_header[subtriBase], &g_tri_data[subtriBase], vidx,
                v0, v1, v2,
                (float2)(bary[0], bary[1]),
                (float2)(bary[i * 2 - 2], bary[i * 2 - 1]),
                (float2)(bary[i * 2 + 0], bary[i * 2 + 1]),
                p0, p1, p2, rcpW,
                d1, d2, area,
                c_viewport_width, c_viewport_height, c_samples_log2, c_render_mode, face, c_primitive_config);

            subtriBase++;
        }

        v1 = v2;
    }

}