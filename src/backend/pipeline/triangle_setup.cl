#include <backend/types.cl>
#include <backend/utils/common.cl>
#include <backend/extensions/cl_khr_global_int32_base_atomics/include.cl>
#include <backend/extensions/cl_khr_local_int32_base_atomics/include.cl>
#include <backend/extensions/cl_khr_local_int32_extended_atomics/include.cl>

#define bary_addr_space private

#define cross2d(a, b) ((a).x * (b).y - (a).y * (b).x)

static uint3 setup_plane_equation(
    const float3 values,
    const int2 v0, const int2 d1, const int2 d2,
    const float areaRcp,
    const int samplesLog2
) {
    // IEEE-754 binary32 layout, used to do float math with integer ops.
    const int  FP32_MANTISSA_BITS = 23;
    const int  FP32_EXPONENT_BIAS = 127;
    const uint FP32_IMPLICIT_ONE  = 1u << FP32_MANTISSA_BITS;
    const uint FP32_MANTISSA_MASK = FP32_IMPLICIT_ONE - 1u;

    const int  VALUE_BITS   = 22;
    const int  MAX_SHIFT    = 8;
    const int  OFFSET_SHIFT = 13;

    const float max_value   = fmax(fmax(values.x, values.y), values.z);
    const int   value_shift = clamp(ilogb(max_value) - VALUE_BITS, 0, MAX_SHIFT);

    const uint3 scaled = convert_uint3(values) >> value_shift;
    const int   base   = scaled.x;
    const int   d_v1   = (int)scaled.y - base;
    const int   d_v2   = (int)scaled.z - base;

    const uint rcp_mant  = (as_uint(areaRcp) & FP32_MANTISSA_MASK) | FP32_IMPLICIT_ONE;
    const int  rcp_shift = (FP32_MANTISSA_BITS + FP32_EXPONENT_BIAS) - (int)(as_uint(areaRcp) >> FP32_MANTISSA_BITS);

    const int subpixel_shift = CR_SUBPIXEL_LOG2 - samplesLog2;
    const int grad_shift     = rcp_shift - (value_shift + subpixel_shift);

    const long2 pixel_grad = {
        ((long)d_v1 * d2.y - (long)d_v2 * d1.y) * rcp_mant,
        ((long)d_v2 * d1.x - (long)d_v1 * d2.x) * rcp_mant,
    };

    uint3 pleq;
    pleq.xy = convert_uint2(pixel_grad >> grad_shift);
    pleq.z  = base << value_shift;

    const int2 center = (v0 * 2 + min(d1, min(d2, 0)) + max(d1, max(d2, 0))) >> (subpixel_shift + 1);
    const int2 frac   = v0 - (center << subpixel_shift);

    const long2 frac_term = (pixel_grad >> OFFSET_SHIFT) * convert_long2(frac);
    pleq.z -= (uint)((frac_term.x + frac_term.y) >> (rcp_shift - value_shift - OFFSET_SHIFT));
    pleq.z -= pleq.x * center.x + pleq.y * center.y;                                      

    return pleq;
}

static int clip_polygon_with_plane(
    bary_addr_space float* bary_out,
    bary_addr_space const float* bary_in,
    const int num_in,
    const float3 plane
) {
    if (num_in < 3) return 0;

    int num_out = 0;

    int    a      = num_in - 1;
    float2 vert_a = (float2)(bary_in[2 * a], bary_in[2 * a + 1]);
    float  dist_a = plane.x + plane.y * vert_a.x + plane.z * vert_a.y;

    for (int b = 0; b < num_in; ++b)
    {
        const float2 vert_b = (float2)(bary_in[2 * b], bary_in[2 * b + 1]);
        const float  dist_b = plane.x + plane.y * vert_b.x + plane.z * vert_b.y;

        if (dist_a * dist_b < 0.0f)
        {
            const float  t = dist_a / (dist_a - dist_b);
            const float2 hit = vert_a + (vert_b - vert_a) * t;
            bary_out[2 * num_out + 0] = hit.x;
            bary_out[2 * num_out + 1] = hit.y;
            num_out++;
        }

        if (dist_b >= 0.0f)
        {
            bary_out[2 * num_out + 0] = vert_b.x;
            bary_out[2 * num_out + 1] = vert_b.y;
            num_out++;
        }

        a      = b;
        vert_a = vert_b;
        dist_a = dist_b;
    }

    return num_out;
}

static inline bool is_triangle_outside_dim(
    const uint dim,
    const float4 v0, const float4 v1, const float4 v2
) {
    return v0.w < fabs(v0[dim]) || v1.w < fabs(v1[dim]) || v2.w < fabs(v2[dim]);
}

static inline int clip_triangle_with_frustum(
    bary_addr_space float* bary,
    const float4 v0, const float4 v1, const float4 v2,
    const float4 d1, const float4 d2
) {
    int num = 3;
    bary[0] = 0.0f; bary[1] = 0.0f;
    bary[2] = 1.0f; bary[3] = 0.0f;
    bary[4] = 0.0f; bary[5] = 1.0f;

    float temp[18];

    #pragma unroll
    for(int dim = 0; dim < 3; ++dim) if (is_triangle_outside_dim(dim, v0, v1, v2))
    {
        // clip for each +/- dim plane
        const float3 w_point   = { v0.w,    d1.w,    d2.w    };
        const float3 dim_point = { v0[dim], d1[dim], d2[dim] };

        num = clip_polygon_with_plane(temp, bary, num, w_point + dim_point);
        num = clip_polygon_with_plane(bary, temp, num, w_point - dim_point);
    }

    return num;
}

static inline void snap_triangle(
    float4 v0, float4 v1, float4 v2,
    int2* p0, int2* p1, int2* p2, float3* rcpW, int2* lo, int2* hi,
    int c_viewport_width, int c_viewport_height 
) {
    float2 view_scale = convert_float2((int2)(c_viewport_width, c_viewport_height) << (CR_SUBPIXEL_LOG2 - 1));

    *rcpW = (float3){1.0f / v0.w, 1.0f / v1.w, 1.0f / v2.w};
  
    *p0 = convert_int2_sat_rte(v0.xy * rcpW->x * view_scale);
    *p1 = convert_int2_sat_rte(v1.xy * rcpW->y * view_scale);
    *p2 = convert_int2_sat_rte(v2.xy * rcpW->z * view_scale);
  
    *lo = (int2){min(p0->x, min(p1->x, p2->x)), min(p0->y, min(p1->y, p2->y))};
    *hi = (int2){max(p0->x, max(p1->x, p2->x)), max(p0->y, max(p1->y, p2->y))};
}


static inline int discard_triangle(
    int2 p0, int2 p1, int2 p2, int2 lo, int2 hi,
    int2* d1, int2* d2, int* area, 
    int c_viewport_width, int c_viewport_height,
    int c_samples_log2
) {
    // backfacing or degenerate => cull

    *d1 = p1 - p0;
    *d2 = p2 - p0;
    *area = cross2d(*d1, *d2); // d1->x * d2->y - d1->y * d2->x;

    // TODO: maybe not required
    if (*area <= 0) return 2;

    // AABB falls between samples => cull.
    int sampleSize = 1 << (CR_SUBPIXEL_LOG2 - c_samples_log2);
    int2 bias = ((int2){c_viewport_width, c_viewport_height} << (CR_SUBPIXEL_LOG2 - 1)) - (sampleSize >> 1);
    int2 low = (lo + bias + (sampleSize - 1)) & -sampleSize;
    int2 high = (hi + bias) & -sampleSize;

    if (low.x > high.x || low.y > high.y) return 3;

    // AABB covers 1 or 2 samples => cull if they are not covered.

    int diff = high.x + high.y - low.x - low.y;
    if (diff <= sampleSize)
    {
        int2 t0 = p0 + bias - low;
        int2 t1 = p1 + bias - low;
        int2 t2 = p2 + bias - low;

        int e0 = cross2d(t0, t1);
        int e1 = cross2d(t1, t2);
        int e2 = cross2d(t2, t0);

        if (e0 < 0 || e1 < 0 || e2 < 0)
        {
            if (diff == 0) return 4;

            t0 = p0 + bias - high;
            t1 = p1 + bias - high;
            t2 = p2 + bias - high;

            e0 = cross2d(t0, t1);
            e1 = cross2d(t1, t2);
            e2 = cross2d(t2, t0);

            if (e0 < 0 || e1 < 0 || e2 < 0) return 5;
        }
    }

    return 0; // visible.
}

static inline void write_triangle_data(
    global triangle_data_t*     restrict tri_data,
    const int3 vidx,
    const float4 v0, const float4 v1, const float4 v2,
    const float2 b0, const float2 b1, const float2 b2,
    const float3 rcpW,
    const int2 p0,
    const float3 zvert,
    const int2 d1, const int2 d2, 
    const int area,
    const int viewport_width, const int viewport_height,
    const int samples_log2, const render_mode_t render_mode
) {
    const float areaRcp = 1.0f / (float) area;

    // window vertex 0 position [0, size)
    const int2 wv0 = p0 + ((int2){viewport_width, viewport_height} << (CR_SUBPIXEL_LOG2 - 1)); 

    // write depth plane equations

    if (is_render_mode_flag_enable_depth(render_mode))
    {
        const int2 zv0 = wv0 - (1 << (CR_SUBPIXEL_LOG2 - samples_log2 - 1));
        const uint3 zpleq = setup_plane_equation(zvert, zv0, d1, d2, areaRcp, samples_log2);

        tri_data->zx = zpleq.x; 
        tri_data->zy = zpleq.y; 
        tri_data->zb = zpleq.z; 

        uint zslope = 0;

        if (samples_log2 != 0)
        {
            uint tmp = abs((int)zpleq.x) + abs(max((int)zpleq.y, -INT_MAX));
            zslope = tmp << max(samples_log2 - 1, 0);
            if ((zslope >> max(samples_log2 - 1, 0)) != tmp) zslope = UINT_MAX;
        }
        
        tri_data->zslope = zslope;
    }
    
    // write lerp plane equations
    
    if (is_render_mode_flag_enable_lerp(render_mode))
    {
        const float wcoef = fmin(fmin(v0.w, v1.w), v2.w) * (float)CR_BARY_MAX;

        const float3 wvert = (float3) (wcoef)            * rcpW;
        const float3 uvert = (float3) {b0.x, b1.x, b2.x} * wvert;
        const float3 vvert = (float3) {b0.y, b1.y, b2.y} * wvert;

        const uint3 wpleq = setup_plane_equation(wvert, wv0, d1, d2, areaRcp, samples_log2 + 1);

        tri_data->wx = wpleq.x; 
        tri_data->wy = wpleq.y; 
        tri_data->wb = wpleq.z;

        const uint3 upleq = setup_plane_equation(uvert, wv0, d1, d2, areaRcp, samples_log2 + 1);

        tri_data->ux = upleq.x; 
        tri_data->uy = upleq.y; 
        tri_data->ub = upleq.z;

        const uint3 vpleq = setup_plane_equation(vvert, wv0, d1, d2, areaRcp, samples_log2 + 1);
        
        tri_data->vx = vpleq.x; 
        tri_data->vy = vpleq.y; 
        tri_data->vb = vpleq.z;
    } else {
        tri_data->vb = 0;
    }
    
    // write vertex indexes

    tri_data->vi0 = vidx.x; 
    tri_data->vi1 = vidx.y; 
    tri_data->vi2 = vidx.z;
}

static inline void write_triangle_header(
    global triangle_header_t*   restrict tri_header,
    const int2 p0, const int2 p1, const int2 p2, 
    const int2 d1, const int2 d2,
    const render_mode_t render_mode,
    const uint      zmin,
    const uint      zmax,
    const face_t    face, 
    const uint      primitive_config
) {
    // determine flipbits

    const uint f01 = cover8x8_selectFlips(d1.x, d1.y);
    const uint f12 = cover8x8_selectFlips(d2.x - d1.x, d2.y - d1.y);
    const uint f20 = cover8x8_selectFlips(-d2.x, -d2.y);

    // set header misc

    triangle_header_misc_t th_misc; // TODO: create a constructor 

    th_misc.misc = (zmin & 0xfffff000u) | (f01 << 6) | (f12 << 2) | (f20 >> 2);
    set_th_misc_face(&th_misc, face);
    set_th_misc_primitive_config(&th_misc, primitive_config);

    // write triangle_header_t

    tri_header->v[0] = prmt(p0.x, p0.y, 0x5410);
    tri_header->v[1] = prmt(p1.x, p1.y, 0x5410);
    tri_header->v[2] = prmt(p2.x, p2.y, 0x5410);
    tri_header->misc = th_misc;
}

static inline void setup_triangle(
    global triangle_header_t*   restrict th, 
    global triangle_data_t*     restrict td, 
    int3 vidx,
    float4 v0, float4 v1, float4 v2,
    float2 b0, float2 b1, float2 b2,
    int2 p0, int2 p1, int2 p2, 
    float3 rcpW,
    int2 d1, int2 d2, 
    int area,
    int c_viewport_width, int c_viewport_height,
    int c_samples_log2, 
    render_mode_t c_render_mode, 
    face_t face, 
    uint c_primitive_config
) {
    // setup depth data

    float3 zvert;
    uint zmin = 0, zmax = -1;

    if (is_render_mode_flag_enable_depth(c_render_mode))
    {
        const float zcoef = (float)(CR_DEPTH_MAX - CR_DEPTH_MIN) * 0.5f;
        const float zbias = (float)(CR_DEPTH_MAX + CR_DEPTH_MIN) * 0.5f;

        zvert = (float3){v0.z, v1.z, v2.z} * zcoef * rcpW + zbias;

        zmin = convert_uint_sat_rte(fmin(fmin(zvert.x, zvert.y), zvert.z) - (float)CR_LERP_ERROR(c_samples_log2));
        zmax = convert_uint_sat_rte(fmax(fmax(zvert.x, zvert.y), zvert.z) + (float)CR_LERP_ERROR(c_samples_log2));
    }

    // write 

    write_triangle_data(td, vidx, v0, v1, v2, b0, b1, b2, rcpW, p0, zvert, d1, d2, area, c_viewport_width, c_viewport_height, c_samples_log2, c_render_mode);

    write_triangle_header(th, p0, p1, p2, d1, d2, c_render_mode, zmin, zmax, face, c_primitive_config);
}


static inline bool cull_view_fustrum(const float4 v0, const float4 v1, const float4 v2)
{
    if (v0.w < fabs(v0.x) || v0.w < fabs(v0.y) || v0.w < fabs(v0.z))
    {
        if ((v0.w < +v0.x && v1.w < +v1.x && v2.w < +v2.x) |
            (v0.w < -v0.x && v1.w < -v1.x && v2.w < -v2.x) |
            (v0.w < +v0.y && v1.w < +v1.y && v2.w < +v2.y) |
            (v0.w < -v0.y && v1.w < -v1.y && v2.w < -v2.y) |
            (v0.w < +v0.z && v1.w < +v1.z && v2.w < +v2.z) |
            (v0.w < -v0.z && v1.w < -v1.z && v2.w < -v2.z) )
        {
            return true;
        }
    }

    return false;
}

static inline face_t get_triangle_face(const float4 v0, const float4 v1, const float4 v2) 
{
    float2 v0w = v0.xy / v0.w;
    float2 v1w = v1.xy / v1.w;
    float2 v2w = v2.xy / v2.w;

    float2 d1 = v1w - v0w;
    float2 d2 = v2w - v0w;

    float area = d1.x * d2.y - d1.y * d2.x;

    return area >= 0 ? FRONT : BACK;
}

static inline bool cull_facing(const face_t face, const render_mode_t render_mode)
{
    if (face == BACK  && is_render_mode_flag_enable_cull_back (render_mode)) return true;
    if (face == FRONT && is_render_mode_flag_enable_cull_front(render_mode)) return true;

    return false;
}

static inline void triangle_setup(
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
    bary_addr_space float* bary,
    const uint c_primitive_config,
    const uint tri_idx
) {
    // read vertices

    const int stride = c_vertex_size / sizeof(float4); // TODO: move to host, maybe c_stride

    float4 v0, v1, v2;
    v0 = read_vertex_buffer(t_vertex_buffer, vidx.x * stride);
    v1 = read_vertex_buffer(t_vertex_buffer, vidx.y * stride);
    v2 = read_vertex_buffer(t_vertex_buffer, vidx.z * stride);

    // outside view frustum or face culling => cull.

    const face_t face = get_triangle_face(v0, v1, v2);

    if (cull_view_fustrum(v0, v1, v2) || cull_facing(face, c_render_mode)) 
    {
        g_tri_subtris[tri_idx] = 0;
        return;
    }

    // check face => flip

    if (face == BACK)
    {
        uint    vidx_T = vidx.x; 
                vidx.x = vidx.z; 
                vidx.z = vidx_T;

        float4  vT = v0;
                v0 = v2;
                v2 = vT;
    } 

    // inside depth range => try to snap vertices.

    if (v0.w >= fabs(v0.z) && v1.w >= fabs(v1.z) && v2.w >= fabs(v2.z))
    {
        // Inside S16 range and small enough => fast path.
        // Note: aabbLimit comes from the fact that cover8x8
        // does not support guardband with maximal viewport.

        int2 p0, p1, p2;    // vertex.xy subpixel on window space 
        int2 lo, hi;        // low and high x and y components
        float3 rcpW;

        snap_triangle(v0, v1, v2, &p0, &p1, &p2, &rcpW, &lo, &hi, c_viewport_width, c_viewport_height);

        const int loxy = min(lo.x, lo.y);
        const int hixy = max(hi.x, hi.y);
        const int aabbLimit = (1 << (CR_MAXVIEWPORT_LOG2 + CR_SUBPIXEL_LOG2)) - 1;

        // inside short range => fast dispatch

        if (loxy >= SHRT_MIN && hixy <= SHRT_MAX && hixy - loxy <= aabbLimit)
        {
            int2 d1, d2;
            int area;

            int discard = discard_triangle(p0, p1, p2, lo, hi, &d1, &d2, &area, c_viewport_width, c_viewport_height, c_samples_log2);

            g_tri_subtris[tri_idx] = discard ? 0 : 1;

            if (!discard)
            {
                setup_triangle(
                    &g_tri_header[tri_idx], 
                    &g_tri_data[tri_idx], 
                    vidx,
                    v0, v1, v2,
                    (float2){0.0f, 0.0f},
                    (float2){1.0f, 0.0f},
                    (float2){0.0f, 1.0f},
                    p0, p1, p2, rcpW,
                    d1, d2, area,
                    c_viewport_width, c_viewport_height,
                    c_samples_log2, c_render_mode, face, c_primitive_config);
            }

            return;
        }
    }

    // Clip to view frustum.

    float4 ov0 = v0;
    float4 od1 = v1 - v0;
    float4 od2 = v2 - v0;
    int num_verts = clip_triangle_with_frustum(bary, ov0, v1, v2, od1, od2);

    v0 = ov0 + od1 * bary[0] + od2 * bary[1];
    v1 = ov0 + od1 * bary[2] + od2 * bary[3];

    float4 tv1 = v1;

    int num_subtris = 0;
    for (int i = 2; i < num_verts; i += 2)
    {
        v2 = ov0 + od1 * bary[i] + od2 * bary[i+1];

        int2 p0, p1, p2;    // vertex.xy subpixel on window space 
        int2 lo, hi;        // low and high x and y components
        float3 rcpW;

        snap_triangle(v0, v1, v2, &p0, &p1, &p2, &rcpW, &lo, &hi, c_viewport_width, c_viewport_height);

        int2 d1, d2;
        int area;

        if (discard_triangle(p0, p1, p2, lo, hi, &d1, &d2, &area, c_viewport_width, c_viewport_height, c_samples_log2) == 0)
            num_subtris++;

        v1 = v2;
    }

    g_tri_subtris[tri_idx] = num_subtris;

    // Multiple subtriangles => allocate.

    int subtri_base = tri_idx;
    if (num_subtris > 1)
    {
        subtri_base = atomic_add(a_num_subtris, num_subtris);
        g_tri_header[tri_idx].misc.misc = subtri_base;
        
        if (subtri_base + num_subtris > c_max_subtris) num_verts = 0;
    }

    // Setup subtriangles.

    v1 = tv1;
    for (int i = 2; i < num_verts; i +=2)
    {
        v2 = ov0 + od1 * bary[i] + od2 * bary[i+1];

        int2 p0, p1, p2;    // vertex.xy subpixel on window space 
        int2 lo, hi;        // low and high x and y components
        float3 rcpW;

        snap_triangle(v0, v1, v2, &p0, &p1, &p2, &rcpW, &lo, &hi, c_viewport_width, c_viewport_height);

        int2 d1, d2;
        int area;

        if (discard_triangle(p0, p1, p2, lo, hi, &d1, &d2, &area, c_viewport_width, c_viewport_height, c_samples_log2) == 0)
        {
            setup_triangle(
                &g_tri_header[subtri_base], &g_tri_data[subtri_base], vidx,
                v0, v1, v2,
                (float2)(bary[0], bary[1]),
                (float2)(bary[i * 2 - 2], bary[i * 2 - 1]),
                (float2)(bary[i * 2 + 0], bary[i * 2 + 1]),
                p0, p1, p2, rcpW,
                d1, d2, area,
                c_viewport_width, c_viewport_height, c_samples_log2, c_render_mode, face, c_primitive_config);

            subtri_base++;
        }

        v1 = v2;
    }

}

kernel __attribute__((reqd_work_group_size(DEVICE_SETUP_THREADS, 1, 1)))
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
    bary_addr_space float bary[18];

    int task_idx = get_global_linear_id();

    if (task_idx >= c_num_tris) return;

    int3 vidx;

    if (is_render_mode_flag_triangle_fan(c_render_mode)) {
        vidx = (int3){0, task_idx + 1, task_idx + 2};
    } else if (is_render_mode_flag_triangle_strip(c_render_mode)) {
        uint offset = 2 * (task_idx % 2);
        vidx = (int3){task_idx + offset, task_idx + 1, task_idx + 2 - offset};
    } else {
        vidx = (int3){task_idx*3 + 0, task_idx*3 + 1, task_idx*3 + 2};
    }

    vidx += c_vertex_offset;

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
        c_primitive_config,
        get_global_id(0)
    );
}

kernel __attribute__((reqd_work_group_size(DEVICE_SETUP_THREADS, 1, 1)))
void triangle_setup_arrays_block(
    global int* a_num_subtris,

    global triangle_header_t* g_tri_header,
    global triangle_data_t* g_tri_data,
    global uchar* g_tri_subtris,

    ro_vertex_buffer_t t_vertex_buffer,

    global const setup_draw_config_t* restrict g_setup,
    const uint c_first_draw,
    const uint c_num_draws,
    const int c_max_subtris,
    const int c_samples_log2,
    const int c_vertex_size,
    const int c_viewport_height,
    const int c_viewport_width
)
{
    bary_addr_space float bary[18];

    const uint tri_begin = g_setup[c_first_draw].tri_start;
    const uint tri_end   = g_setup[c_num_draws].tri_start;
    const uint task_idx  = get_global_id(0) + tri_begin;

    if (task_idx >= tri_end) return;

    uint instance = c_first_draw;
    for (uint hi = c_num_draws; hi - instance > 1; ) {
        uint mid = instance + ((hi - instance) >> 1);
        if (g_setup[mid].tri_start <= task_idx) instance = mid; else hi = mid;
    }

    const setup_draw_config_t cfg = g_setup[instance];
    const render_mode_t c_render_mode = (render_mode_t){ cfg.render_mode };

    const uint vstart    = cfg.vertex_start;
    const uint local_tri = task_idx - cfg.tri_start;

    int3 vidx;

    if (is_render_mode_flag_triangle_fan(c_render_mode)) {
        vidx = (int3){vstart, vstart + local_tri + 1, vstart + local_tri + 2};
    } else if (is_render_mode_flag_triangle_strip(c_render_mode)) {
        uint offset = 2 * (local_tri % 2);
        vidx = (int3){vstart + local_tri + offset, vstart + local_tri + 1, vstart + local_tri + 2 - offset};
    } else {
        vidx = (int3){vstart + local_tri*3, vstart + local_tri*3 + 1, vstart + local_tri*3 + 2};
    }

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
        cfg.primitive_config,
        task_idx
    );
}

kernel __attribute__((reqd_work_group_size(DEVICE_SETUP_THREADS, 1, 1)))
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
    bary_addr_space float bary[18];

    int task_idx = get_global_linear_id();
    
    if (task_idx >= c_num_tris)
        return;
    
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
        c_primitive_config,
        get_global_id(0)
    );
}
