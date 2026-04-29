#ifndef TYPES_DEVICE_H
#define TYPES_DEVICE_H

#define CL_TARGET_OPENCL_VERSION 120

#ifdef __COMPILER_RELATIVE_PATH__
#include <constants.device.h>
#else
#include "constants.device.h"
#endif

// defining primitives for c and cl context
#ifdef __OPENCL_VERSION__
typedef bool    cl_bool;
typedef uchar   cl_uchar;
typedef uchar2   cl_uchar2;
typedef short   cl_short;
typedef ushort  cl_ushort;
typedef ushort2  cl_ushort2;
typedef int     cl_int;
typedef uint    cl_uint;
typedef uint2   cl_uint2;
typedef uint4   cl_uint4;
typedef ulong   cl_ulong;
typedef ulong2   cl_ulong2;
#else
#include <CL/opencl.h>
#include <math.h>
#endif

#ifdef DEVICE_SUB_GROUP_ENABLED
typedef struct
{
#if (DEVICE_SUB_GROUP_THREADS <= 8)
    cl_uchar mask;
#elif (DEVICE_SUB_GROUP_THREADS <= 16)
    cl_ushort mask;
#elif (DEVICE_SUB_GROUP_THREADS <= 32)
    cl_uint mask;
#elif (DEVICE_SUB_GROUP_THREADS <= 64)
    cl_ulong mask;
#elif (DEVICE_SUB_GROUP_THREADS <= 128)
    cl_uint4 mask;
#else
#error DEVICE_SUB_GROUP_THREADS too large to be supported.
#endif
} sub_group_mask_t;
#endif

// render mode methods
typedef struct
{
    cl_uint flags;
} render_mode_t;

static inline cl_bool is_render_mode_flag_triangle_fan(const render_mode_t render_mode)
{
    return (render_mode.flags & RENDER_MODE_FLAG_TRIANGLE_FAN) != 0;
}

static inline cl_bool is_render_mode_flag_triangle_strip(const render_mode_t render_mode)
{
    return (render_mode.flags & RENDER_MODE_FLAG_TRIANGLE_STRIP) != 0;
}

inline cl_bool is_render_mode_flag_enable_depth(const render_mode_t render_mode)
{
    return (render_mode.flags & RENDER_MODE_FLAG_ENABLE_DEPTH) != 0;
}

inline cl_bool is_render_mode_flag_enable_stencil(const render_mode_t render_mode) {
    return (render_mode.flags & RENDER_MODE_FLAG_ENABLE_STENCIL) != 0;
}

inline cl_bool is_render_mode_flag_enable_lerp(const render_mode_t render_mode)
{
    return (render_mode.flags & RENDER_MODE_FLAG_ENABLE_LERP) != 0;
}

inline cl_bool is_render_mode_flag_enable_cull_front(const render_mode_t render_mode)
{
    return (render_mode.flags & RENDER_MODE_FLAG_ENABLE_CULL_FRONT) != 0;
}

static inline cl_bool is_render_mode_flag_enable_cull_back(const render_mode_t render_mode)
{
    return (render_mode.flags & RENDER_MODE_FLAG_ENABLE_CULL_BACK) != 0;
}

typedef enum
{
    FRONT = 0,
    BACK = 1
} face_t;

// triangle header misc type
// stores metadata related with the primitive
#define TH_MISC_FACE_POSITION 31
#define TH_MISC_BITFLIPS 12

#define TH_MISC_PRIMITIVE_CONFIG_POSITION (TH_MISC_FACE_POSITION - TRIANGLE_PRIMITIVE_CONFIGS_LOG2)
#define TH_MISC_DEPTH_SIZE_LOG ((TH_MISC_PRIMITIVE_CONFIG_POSITION - TH_MISC_BITFLIPS) / 2)

#define TH_MISC_ZMAX_POSITION (TH_MISC_PRIMITIVE_CONFIG_POSITION - TH_MISC_DEPTH_SIZE_LOG)
#define TH_MISC_ZMIN_POSITION (TH_MISC_BITFLIPS)

typedef struct
{
    cl_uint misc;
} triangle_header_misc_t;

inline face_t get_th_misc_face(const triangle_header_misc_t th_misc)
{
    return (face_t)(th_misc.misc >> TH_MISC_FACE_POSITION);
}
inline cl_uint get_th_misc_primitive_config(const triangle_header_misc_t th_misc)
{
    return (th_misc.misc >> TH_MISC_PRIMITIVE_CONFIG_POSITION) & (TRIANGLE_PRIMITIVE_CONFIGS - 1);
}
inline cl_ushort get_th_misc_zmax(const triangle_header_misc_t th_misc)
{
    const cl_uint LOST_BITS = 16 - (TH_MISC_PRIMITIVE_CONFIG_POSITION - TH_MISC_ZMAX_POSITION);
    cl_uint depth = (th_misc.misc >> (TH_MISC_ZMAX_POSITION - LOST_BITS)) | (LOST_BITS - 1); // bound to upper
    return depth & 0xFFFFu;
}
inline cl_ushort get_th_misc_zmin(const triangle_header_misc_t th_misc)
{
    const cl_uint LOST_BITS = 16 - (TH_MISC_ZMAX_POSITION - TH_MISC_ZMIN_POSITION);
    cl_uint depth = (th_misc.misc >> (TH_MISC_ZMIN_POSITION - LOST_BITS)) & ~(LOST_BITS - 1); // bound to lower
    return depth & 0xFFFFu;
}

inline void set_th_misc_face(triangle_header_misc_t *th_misc, face_t face)
{
    th_misc->misc &= (1u << TH_MISC_FACE_POSITION) - 1;
    th_misc->misc |= ((cl_uint)face << TH_MISC_FACE_POSITION);
}
inline void set_th_misc_primitive_config(triangle_header_misc_t *th_misc, cl_uint primitive_config)
{
    th_misc->misc &= ~((cl_uint)(TRIANGLE_PRIMITIVE_CONFIGS - 1) << TH_MISC_PRIMITIVE_CONFIG_POSITION);
    th_misc->misc |= (primitive_config << TH_MISC_PRIMITIVE_CONFIG_POSITION);
}
inline void set_th_misc_zmax(triangle_header_misc_t *th_misc, cl_ushort zmax)
{
    const cl_uint VALID_BITS = (TH_MISC_PRIMITIVE_CONFIG_POSITION - TH_MISC_ZMAX_POSITION);
    const cl_uint LOST_BITS = 16 - VALID_BITS;
    th_misc->misc &= ~(VALID_BITS << TH_MISC_ZMAX_POSITION);
    th_misc->misc |= ((cl_uint)zmax >> LOST_BITS) << TH_MISC_ZMAX_POSITION;
};
inline void set_th_misc_zmin(triangle_header_misc_t *th_misc, cl_ushort zmin)
{
    const cl_uint VALID_BITS = (TH_MISC_ZMAX_POSITION - TH_MISC_ZMIN_POSITION);
    const cl_uint LOST_BITS = 16 - VALID_BITS;
    th_misc->misc &= ~(VALID_BITS << TH_MISC_ZMIN_POSITION);
    th_misc->misc |= ((cl_uint)zmin >> LOST_BITS) << TH_MISC_ZMIN_POSITION;
};

typedef struct
{
    cl_short v0x; // Subpixels relative to viewport center. Valid if triSubtris = 1.
    cl_short v0y;
    cl_short v1x;
    cl_short v1y;
    cl_short v2x;
    cl_short v2y;

    triangle_header_misc_t misc; // triSubtris=1: (zmin:20, f01:4, f12:4, f20:4), triSubtris>=2: (subtriBase)
} triangle_header_t;

typedef struct
{
    cl_uint zx; // zx * sampleX + zy * sampleY + zb = lerp(CR_DEPTH_MIN, CR_DEPTH_MAX, (clipZ / clipW + 1) / 2)
    cl_uint zy;
    cl_uint zb;
    cl_uint zslope; // (abs(zx) + abs(zy)) * (samplesPerPixel / 2)

    int wx; // wx * (sampleX * 2 + 1) + wy * (sampleY * 2 + 1) + wb = minClipW / clipW * CR_BARY_MAX
    int wy;
    int wb;

    int ux; // ux * (sampleX * 2 + 1) + uy * (sampleY * 2 + 1) + ub = baryU * minClipW / clipW * CR_BARY_MAX
    int uy;
    int ub;

    int vx; // vx * (sampleX * 2 + 1) + vy * (sampleY * 2 + 1) + vb = baryV * minClipW / clipW * CR_BARY_MAX
    int vy;
    int vb;

    unsigned int vi0; // Vertex indices.
    unsigned int vi1;
    unsigned int vi2;
} triangle_data_t;

typedef struct
{
    unsigned int offset;
    unsigned int stride;
    unsigned int misc; // size[3], type[3], normalized[1], vertex_attrib_pointer_active[1]
} vertex_attribute_data_t;

static inline cl_uint gl_get_vertex_attribute_type(vertex_attribute_data_t va_data)
{
    return va_data.misc & VERTEX_ATTRIBUTE_TYPE_MASK;
}
static inline cl_uint gl_get_vertex_attribute_size(vertex_attribute_data_t va_data)
{
    return (va_data.misc & VERTEX_ATTRIBUTE_SIZE_MASK) >> 3;
}
static inline cl_bool gl_get_vertex_attribute_normalize(vertex_attribute_data_t va_data)
{
    return (va_data.misc & VERTEX_ATTRIBUTE_NORMALIZE) != 0;
}

static inline void set_vertex_attribute_type(vertex_attribute_data_t *va_data, cl_uint type)
{
    va_data->misc &= ~VERTEX_ATTRIBUTE_TYPE_MASK;
    va_data->misc |= type & VERTEX_ATTRIBUTE_TYPE_MASK;
}

static inline void set_vertex_attribute_size(vertex_attribute_data_t *va_data, cl_uint size)
{
    va_data->misc &= ~VERTEX_ATTRIBUTE_SIZE_MASK;
    va_data->misc |= (size << 3) & VERTEX_ATTRIBUTE_SIZE_MASK;
}

static inline void set_vertex_attribute_normalize(vertex_attribute_data_t *va_data, cl_bool normalize)
{
    if (normalize) 
    {
        va_data->misc |= VERTEX_ATTRIBUTE_NORMALIZE;
    } 
    else 
    {
        va_data->misc &= ~VERTEX_ATTRIBUTE_NORMALIZE;
    }
}

static inline void set_vertex_attribute_pointer(vertex_attribute_data_t *va_data, cl_bool pointer)
{
    if (pointer) 
    {
        va_data->misc |= VERTEX_ATTRIBUTE_ACTIVE_POINTER;
    } 
    else 
    {
        va_data->misc &= ~VERTEX_ATTRIBUTE_ACTIVE_POINTER;
    }
}

static void set_vertex_attribute(
    vertex_attribute_data_t *va_data, 
    cl_uint offset, 
    cl_uint stride, 
    cl_uint type, 
    cl_uint size, 
    cl_bool normalize,
    cl_bool pointer
) {
    *va_data = (vertex_attribute_data_t) {
        .offset = offset,
        .stride = stride,
        .misc = 0
    };
    set_vertex_attribute_type(va_data, type);
    set_vertex_attribute_size(va_data, size);
    set_vertex_attribute_normalize(va_data, normalize);
    set_vertex_attribute_pointer(va_data, pointer);
}

/*
typedef struct
{
    unsigned int blending : 1;
    unsigned int cull_back : 1;
    unsigned int cull_front : 1;
    unsigned int depth_test : 1;
    unsigned int dithering : 1;
    unsigned int lerp  : 1;
    unsigned int scissor_test : 1;
    unsigned int stencil_test : 1;
    unsigned int polygon_offset_fill : 1;
} render_mode_t;
*/


typedef struct
{
    cl_uint misc;
} blending_data_t;

static void set_blending_data_equation(blending_data_t *blending_data, cl_uint rgb, cl_uint alpha)
{
    blending_data->misc &= ~(0xFu << 0);
    blending_data->misc |= (rgb & 0x3u) << 0;
    blending_data->misc |= (alpha & 0x3u) << 2;
}

static void set_blending_data_function_src(blending_data_t *blending_data, cl_uint rgb, cl_uint alpha)
{
    blending_data->misc &= ~(0xFFu << 16);
    blending_data->misc |= (rgb & 0xFu) << 16;
    blending_data->misc |= (alpha & 0xFu) << 20;
}

static void set_blending_data_function_dst(blending_data_t *blending_data, cl_uint rgb, cl_uint alpha)
{
    blending_data->misc &= ~(0xFFu << 24);
    blending_data->misc |= (rgb & 0xFu) << 24;
    blending_data->misc |= (alpha & 0xFu) << 28;
}

static void set_blending_data(
    blending_data_t *blending_data, 
    cl_uint rgb_equation,
    cl_uint alpha_equation,
    cl_uint rgb_src_func,
    cl_uint alpha_src_func,
    cl_uint rgb_dst_func,
    cl_uint alpha_dst_func
) {
    set_blending_data_equation(blending_data, rgb_equation, alpha_equation);
    set_blending_data_function_src(blending_data, rgb_src_func,  alpha_src_func);
    set_blending_data_function_dst(blending_data, rgb_dst_func,  alpha_dst_func);
}

typedef struct
{
    cl_uint front_misc;
    cl_uint back_misc;
} gl_stencil_data_t;

static void set_stencil_data_func(gl_stencil_data_t *stencil_data, face_t face, cl_uint func, cl_uint mask, cl_uint ref)
{
    cl_uint *face_misc = face == FRONT ? &stencil_data->front_misc : &stencil_data->back_misc;
    *face_misc &= ~(0x7u << 0);
    *face_misc |= (func & 0x7u) << 0;

    *face_misc &= ~(0xFFu << 16);
    *face_misc |= (mask & 0xFFu) << 16;

    *face_misc &= ~(0xFFu << 24);
    *face_misc |= (ref & 0xFFu) << 24;
}

static void set_stencil_data_op(gl_stencil_data_t *stencil_data, face_t face, cl_uint sfail, cl_uint dpass, cl_uint dpfail)
{
    cl_uint *face_misc = face == FRONT ? &stencil_data->front_misc : &stencil_data->back_misc;
    *face_misc &= ~(0x7u << 4);
    *face_misc |= (sfail & 0x7u) << 4;

    *face_misc &= ~(0x7u << 8);
    *face_misc |= (dpass & 0x7u) << 8;

    *face_misc &= ~(0x7u << 12);
    *face_misc |= (dpfail & 0x7u) << 12;
}

typedef struct
{
    cl_uint misc;
    cl_ushort near, far; // near[16], far[16]
} depth_data_t;

static void set_depth_data_func(depth_data_t *depth_data, cl_uint func)
{
    depth_data->misc &= ~(0x7u << 0);
    depth_data->misc |= (func & 0x7u) << 0;
}

static void set_depth_data_range(depth_data_t *depth_data, cl_ushort near, cl_ushort far)
{
    depth_data->near = near;
    depth_data->far = far;
}

static depth_data_t get_depth_data(cl_uint func, cl_ushort near, cl_ushort far) 
{
    depth_data_t depth_data = { .misc = 0, .near = 0, .far = 0 };

    set_depth_data_func(&depth_data, func);
    set_depth_data_range(&depth_data, near, far);

    return depth_data;
}

//--------------------------------------------------------------------------------

typedef struct
{
    cl_ushort misc;
} enabled_data_t;

static cl_uint get_enabled_red_data(enabled_data_t enabled_data)
{
    return (enabled_data.misc >> 8) & 0x1u;
}

static cl_uint get_enabled_green_data(enabled_data_t enabled_data)
{
    return (enabled_data.misc >> 9) & 0x1u;
}

static cl_uint get_enabled_blue_data(enabled_data_t enabled_data)
{
    return (enabled_data.misc >> 10) & 0x1u;
}

static cl_uint get_enabled_alpha_data(enabled_data_t enabled_data)
{
    return (enabled_data.misc >> 11) & 0x1u;
}

static cl_bool get_enabled_depth_data(enabled_data_t enabled_data)
{
    return (enabled_data.misc >> 12) & 0x1u;
}

static cl_uchar get_enabled_stencil_data(enabled_data_t enabled_data)
{
    return (enabled_data.misc >> 0) & 0xFFu;
}

static cl_bool is_enabled_data_all_color_channels(enabled_data_t enabled_data)
{
    return (enabled_data.misc & ENABLED_COLOR_CHANNEL_MASK) == ENABLED_COLOR_CHANNEL_MASK;
}

static cl_bool is_enabled_data_all_stencil_bits(enabled_data_t enabled_data)
{
    return (enabled_data.misc & ENABLED_STENCIL_CHANNEL_MASK) == ENABLED_STENCIL_CHANNEL_MASK;
}

static void set_enabled_data_red_enable(enabled_data_t* enabled_data)
{
    enabled_data->misc |= 0x1u << 8;
}

static void set_enabled_data_green_enable(enabled_data_t* enabled_data)
{
    enabled_data->misc |= 0x1u << 9;
}

static void set_enabled_data_blue_enable(enabled_data_t* enabled_data)
{
    enabled_data->misc |= 0x1u << 10;
}

static void set_enabled_data_alpha_enable(enabled_data_t* enabled_data)
{
    enabled_data->misc |= 0x1u << 11;
}

static void set_enabled_color_data(enabled_data_t *enabled_data, cl_bool red, cl_bool green, cl_bool blue, cl_bool alpha)
{
    enabled_data->misc &= ~(0xFu << 8);
    enabled_data->misc |= (red & 0x1u) << 8;
    enabled_data->misc |= (green & 0x1u) << 9;
    enabled_data->misc |= (blue & 0x1u) << 10;
    enabled_data->misc |= (alpha & 0x1u) << 11;
}
static void set_enabled_stencil_data(enabled_data_t *enabled_data, cl_uchar mask)
{
    enabled_data->misc &= ~(0xFFu << 0);
    enabled_data->misc |= mask;
}
static void set_enabled_depth_data(enabled_data_t *enabled_data, cl_bool mask)
{
    enabled_data->misc &= ~(0x1u << 12);
    enabled_data->misc |= ((cl_ushort)mask & 0x1u) << 12;
}

static enabled_data_t get_enabled_data(
    cl_bool red, cl_bool green, cl_bool blue, cl_bool alpha,
    cl_bool depth,
    cl_uchar stencil
) {
    enabled_data_t enabled_data = {.misc = 0};

    set_enabled_color_data(&enabled_data, red, green, blue, alpha);
    set_enabled_depth_data(&enabled_data, depth);
    set_enabled_stencil_data(&enabled_data, stencil);

    return enabled_data;
}

typedef struct {
    cl_uint misc;
} rgba8_t;

static cl_uint get_rgba8_red(rgba8_t color)
{
    return (color.misc >> 0) & 0xFF;
}

static void set_rgba8_red(rgba8_t* color, cl_uint value)
{
    color->misc &= 0xFFFFFF00u; 
    color->misc |= (value & 0xFFu) << 0;
}

static cl_uint get_rgba8_green(rgba8_t color)
{
    return (color.misc >> 8) & 0xFF;
}

static void set_rgba8_green(rgba8_t* color, cl_uint value)
{
    color->misc &= 0xFFFF00FFu; 
    color->misc |= (value & 0xFFu) << 8;
}

static cl_uint get_rgba8_blue(rgba8_t color)
{
    return (color.misc >> 16) & 0xFF;
}

static void set_rgba8_blue(rgba8_t* color, cl_uint value)
{
    color->misc &= 0xFF00FFFFu; 
    color->misc |= (value & 0xFFu) << 16;
}

static cl_uint get_rgba8_alpha(rgba8_t color)
{
    return (color.misc >> 24) & 0xFF;
}

static void set_rgba8_alpha(rgba8_t* color, cl_uint value)
{
    color->misc &= 0x00FFFFFFu; 
    color->misc |= (value & 0xFFu) << 24;
}

static rgba8_t get_rgba8(cl_uint red, cl_uint green, cl_uint blue, cl_uint alpha)
{
    rgba8_t color = {.misc = 0};

    set_rgba8_red(&color, red);
    set_rgba8_green(&color, green);
    set_rgba8_blue(&color, blue);
    set_rgba8_alpha(&color, alpha);
        
    return color;
}

typedef struct {
    cl_ushort misc;
} depth16_t;

static void set_depth16(depth16_t* depth, cl_short value)
{
    depth->misc = value;
}

typedef struct {
    cl_uchar misc;
} stencil8_t;

typedef struct {
    rgba8_t color;
    depth16_t depth;
    stencil8_t stencil;
} clear_data_t;

typedef struct
{
    render_mode_t render_mode;
    blending_data_t blending_data;
    rgba8_t blending_color;
    cl_uint stencil_data;
    cl_uint depth_data;
    enabled_data_t enabled_data;
} rop_config_t;

// ----------------------------------------------------------
// Texture Data Type 
// ----------------------------------------------------------

// texture_size_t definition

#if DEVICE_MAX_TEXTURE_SIZE_LOG2 <= 8
typedef cl_uchar2 texture_size_t;
#elif DEVICE_MAX_TEXTURE_SIZE_LOG2 <= 16
typedef cl_ushort2 texture_size_t;
#elif DEVICE_MAX_TEXTURE_SIZE_LOG2 <= 32
typedef cl_uint2 texture_size_t;
#elif DEVICE_MAX_TEXTURE_SIZE_LOG2 <= 64
typedef cl_ulong2 texture_size_t;
#else
#error DEVICE_MAX_TEXTURE_SIZE_LOG2 > 64 do not supported
#endif

// texture_data_t definition

typedef struct
{
    /*
    int internalformat : 4;
    int wrap_s : 2;
    int wrap_t : 2;
    int min_filter : 3;
    int mag_filter : 1;
    */
    #ifndef DEVICE_IMAGE_ENABLED
    texture_size_t size;
    #endif
    cl_ushort misc;
} texture_data_t;

// texture data getters

static cl_uint get_texture_data_mode(texture_data_t texture_data)              { return (texture_data.misc >> 0) & 0xFu; }
static cl_uint get_texture_data_wrap_s(texture_data_t texture_data)            { return (texture_data.misc >> 4) & 0x3u; }
static cl_uint get_texture_data_wrap_t(texture_data_t texture_data)            { return (texture_data.misc >> 6) & 0x3u; }
static cl_uint get_texture_data_min_filter(texture_data_t texture_data)        { return (texture_data.misc >> 8) & 0x7u; }
static cl_uint get_texture_data_mag_filter(texture_data_t texture_data)        { return (texture_data.misc >> 11) & 0x1u; }
static cl_int  get_texture_data_max_level(texture_data_t texture_data)         { return 0; } // NOT IMPLEMENTED

// texture data setters

static void set_texture_data_mode(texture_data_t *texture_data, cl_ushort mode)
{
    texture_data->misc = (texture_data->misc & ~0xFu) | (mode & 0xFu);
}

static void set_texture_data_wrap_s(texture_data_t *texture_data, cl_ushort wrap_s)
{
    texture_data->misc &= ~(0x3u << 4);
    texture_data->misc |= wrap_s << 4;
}

static void set_texture_data_wrap_t(texture_data_t *texture_data, cl_ushort wrap_t)
{
    texture_data->misc &= ~(0x3u << 6);
    texture_data->misc |= wrap_t << 6;
}

static void set_texture_data_min_filter(texture_data_t *texture_data, cl_ushort min_filter)
{
    texture_data->misc &= ~(0x7u << 8);
    texture_data->misc |= (min_filter & 0x7u) << 8;
}

static void set_texture_data_mag_filter(texture_data_t *texture_data, cl_ushort mag_filter)
{
    texture_data->misc &= ~(0x1u << 11);
    texture_data->misc |= (mag_filter & 0x1u) << 11;
}

// texture data utils

static inline cl_bool is_texture_data_linear(texture_data_t texture_data)
{
    cl_uint min_filter = get_texture_data_min_filter(texture_data);

    switch (min_filter)
    {
    case TEXTURE_FILTER_LINEAR:
    case TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
    case TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
        return 1;
    default:
        return 0;
    }
}

static inline cl_bool is_texture_data_mipmapping(texture_data_t texture_data)
{
    cl_uint min_filter = get_texture_data_min_filter(texture_data);

    switch (min_filter)
    {
    case TEXTURE_FILTER_NEAREST:
    case TEXTURE_FILTER_LINEAR:
        return 0;
    default:
        return 1;
    }
}

static inline cl_bool is_texture_data_mipmapping_linear(texture_data_t texture_data)
{
    cl_uint min_filter = get_texture_data_min_filter(texture_data);

    switch (min_filter)
    {
    case TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
    case TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
        return 1;
    default:
        return 0;
    }
}

static inline void set_texture_data_size(texture_data_t* texture_data, texture_size_t size)
{
    #ifndef DEVICE_IMAGE_ENABLED
    texture_data->size = size;
    #endif
}

static inline texture_size_t get_texture_data_size(texture_data_t texture_data)
{
    #ifndef DEVICE_IMAGE_ENABLED
    return texture_data.size;
    #else
    return (texture_size_t){0,0};
    #endif
}

static inline cl_int  get_texture_data_color_size(texture_data_t texture_data)
{
    int mode = get_texture_data_mode(texture_data);

    switch (mode)
    {
    default:
    case TEX_R8:
        return sizeof(cl_uchar);
    case TEX_RG8:
    case TEX_RGBA4:
    case TEX_RGB565:
    case TEX_RGB5_A1:
        return sizeof(cl_uchar[2]);
    case TEX_RGB8:
        return sizeof(cl_uchar[3]);
    case TEX_RGBA8:
        return sizeof(cl_uchar[4]);
    }
}

// SW support for image addressing

static cl_bool get_texture_data_require_clamp_x(texture_data_t texture_data)
{
    cl_uint wrap_s = get_texture_data_wrap_s(texture_data);

    #ifdef DEVICE_IMAGE_ENABLED
    {
        cl_uint wrap_t = get_texture_data_wrap_t(texture_data);

        return wrap_s == TEXTURE_WRAP_CLAMP_TO_EDGE && wrap_t != TEXTURE_WRAP_CLAMP_TO_EDGE; 
    }
    #else
    {
        return wrap_s == TEXTURE_WRAP_CLAMP_TO_EDGE;
    }
    #endif
}

static cl_bool get_texture_data_require_clamp_y(texture_data_t texture_data)
{
    cl_uint wrap_t = get_texture_data_wrap_t(texture_data);

    #ifdef DEVICE_IMAGE_ENABLED
    {
        cl_uint wrap_s = get_texture_data_wrap_s(texture_data);

        return wrap_t == TEXTURE_WRAP_CLAMP_TO_EDGE && wrap_s != TEXTURE_WRAP_CLAMP_TO_EDGE; 
    }
    #else
    {
        return wrap_t == TEXTURE_WRAP_CLAMP_TO_EDGE;
    }
    #endif
}

static cl_bool get_texture_data_require_negate_x(texture_data_t texture_data, float x)
{
    cl_uint wrap_s = get_texture_data_wrap_s(texture_data);

    #ifdef DEVICE_IMAGE_ENABLED
    {
        cl_uint wrap_t = get_texture_data_wrap_t(texture_data);

        return wrap_t == TEXTURE_WRAP_MIRRORED_REPEAT && wrap_s == TEXTURE_WRAP_REPEAT && fmod(x,2.f) == 1; 
    }
    #else
    {
        return wrap_s != TEXTURE_WRAP_CLAMP_TO_EDGE && x < 0.f;
    }
    #endif
}

static cl_bool get_texture_data_require_negate_y(texture_data_t texture_data, float y)
{
    cl_uint wrap_t = get_texture_data_wrap_t(texture_data);
    
    #ifdef DEVICE_IMAGE_ENABLED
    {
        cl_uint wrap_s = get_texture_data_wrap_s(texture_data);

        return wrap_s == TEXTURE_WRAP_MIRRORED_REPEAT && wrap_t == TEXTURE_WRAP_REPEAT && fmod(y,2.f) == 1; 
    }
    #else
    {
        return wrap_t != TEXTURE_WRAP_CLAMP_TO_EDGE && y < 0.f;
    }
    #endif
}

static cl_bool get_texture_data_require_diff_x(texture_data_t texture_data, float x)
{
    #ifdef DEVICE_IMAGE_ENABLED
    {
        return 0;
    }
    #else
    {
        cl_uint wrap_s = get_texture_data_wrap_s(texture_data);
        
        return wrap_s == TEXTURE_WRAP_MIRRORED_REPEAT && fmod(x,2.f) == 1;
    }
    #endif 
}

static cl_bool get_texture_data_require_diff_y(texture_data_t texture_data, float y)
{
    #ifdef DEVICE_IMAGE_ENABLED
    {
        return 0;
    }
    #else
    {
        cl_uint wrap_t = get_texture_data_wrap_t(texture_data);
        
        return wrap_t == TEXTURE_WRAP_MIRRORED_REPEAT && fmod(y,2.f) == 1;
    }
    #endif
}

static cl_bool get_texture_data_require_software_support(texture_data_t texture_data)
{
    int mode = get_texture_data_mode(texture_data);

    switch(mode)
    {
        case TEX_RGBA4:
        case TEX_RGB5_A1:
            return 1;
        default:
        return 0;
    }
}


// ----------------------------------------------------------
// FRAMEBUFFER
// ----------------------------------------------------------

typedef struct
{
    cl_uchar misc;
} gl_framebuffer_data_t;

static void set_framebuffer_data_colorbuffer_enabled(gl_framebuffer_data_t *framebuffer_data)
{
    framebuffer_data->misc |= (0x1u << 0);
}

static void set_framebuffer_data_depthbuffer_enabled(gl_framebuffer_data_t *framebuffer_data)
{
    framebuffer_data->misc |= (0x1u << 1);
}

static void set_framebuffer_data_stencilbuffer_enabled(gl_framebuffer_data_t *framebuffer_data)
{
    framebuffer_data->misc |= (0x1u << 2);
}

static void set_framebuffer_data_colorbuffer_disabled(gl_framebuffer_data_t *framebuffer_data)
{
    framebuffer_data->misc &= ~(0x1u << 0);
}

static void set_framebuffer_data_depthbuffer_disabled(gl_framebuffer_data_t *framebuffer_data)
{
    framebuffer_data->misc &= ~(0x1u << 1);
}

static void set_framebuffer_data_stencilbuffer_disabled(gl_framebuffer_data_t *framebuffer_data)
{
    framebuffer_data->misc &= ~(0x1u << 2);
}

static cl_uchar is_framebuffer_data_colorbuffer_enabled(gl_framebuffer_data_t framebuffer_data)
{
    return framebuffer_data.misc & (0x1u << 0);
}

static cl_uchar is_framebuffer_data_depthbuffer_enabled(gl_framebuffer_data_t framebuffer_data)
{
    return framebuffer_data.misc & (0x1u << 1);
}

static cl_uchar is_framebuffer_data_stencilbuffer_enabled(gl_framebuffer_data_t framebuffer_data)
{
    return framebuffer_data.misc & (0x1u << 2);
}

// uniform type

typedef struct {
    cl_uchar __attribute__((aligned(DEVICE_MEM_BASE_ADDR_ALIGN))) data[DEVICE_UNIFORM_CAPACITY];
} uniform_buffer_t;

typedef uniform_buffer_t uniform_buffer_array_t[TRIANGLE_PRIMITIVE_CONFIGS] __attribute__((aligned(DEVICE_MEM_BASE_ADDR_ALIGN)));

#endif