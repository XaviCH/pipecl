#ifndef BACKEND_TYPES_CL
#define BACKEND_TYPES_CL

#include <types.device.h>

#ifdef DEVICE_POINTER_REINTERPRET_ENABLED
    typedef global const void* ro_buffer_t;
    typedef global       void* rw_buffer_t;
#else
    typedef global const uchar* ro_buffer_t;
    typedef global       uchar* rw_buffer_t;
#endif

#if DEVICE_UNIFORM_CONSTANT_MEM_SUPPORT == 1
    #define uniform_addr_space constant
#else
    #define uniform_addr_space global const
#endif

#ifdef DEVICE_IMAGE_ENABLED
    typedef read_only  image1d_buffer_t ro_vertex_buffer_t;
    typedef write_only image1d_buffer_t wo_vertex_buffer_t;
    typedef read_only  image2d_t        ro_texture2d_t;
#else
    typedef global const float4* restrict ro_vertex_buffer_t;
    typedef global       float4* restrict wo_vertex_buffer_t;
    typedef ro_buffer_t          restrict ro_texture2d_t;
#endif

#if DEVICE_RW_IMAGE_ENABLED
    typedef read_write image2d_t rw_texture2d_t;
    typedef read_write image2d_t colorbuffer_t;
    typedef read_write image2d_t depthbuffer_t;
    typedef read_write image2d_t stencilbuffer_t;
#else
    typedef rw_buffer_t    restrict rw_texture2d_t;
    typedef rw_buffer_t    restrict colorbuffer_t;
    typedef global ushort* restrict depthbuffer_t;
    typedef global uchar*  restrict stencilbuffer_t;
#endif

// DATA functions

/**
 * Decompress a uint color to uint4 channel color.
 * @deprecated Use rgba_t functions
 */
static inline uint4 uint_to_uint4(const int data, int mode);

/**
 * Transforms a uint4 channel color to an standard rgba float color from GL specs.
 * @todo encapsulate float4 into color type. maybe 32 bit floating point presition is not required.
 */
static inline float4 uint4_to_float4_color(uint4 color, int mode);

/**
 * Compress a uint4 channel color to an standard rgba8 uint.
 * @deprecated Use rgba_t functions
 */
static inline uint uint4_to_uint(const uint4 data);

// ------------------------------------------------------------------------------
// Implementation
// ------------------------------------------------------------------------------

// TODO: this does not support al image types
static inline uint4 uint_to_uint4(const int data, int mode)
{
    uint4 color;

    switch (mode) 
    {
        case TEX_RGBA8:
        case TEX_RGBA4:
        case TEX_RGB5_A1:
        case TEX_RGB565:
            color.w = (data >> 24) & 0xFFu;
        case TEX_RGB8:
            color.z = (data >> 16) & 0xFFu;
        case TEX_RG8:
            color.y = (data >>  8) & 0xFFu;
        default:
        case TEX_R8:
            color.x = (data >>  0) & 0xFFu;
            break;
    }

    return color;
}

// TODO: this does not support al image types
static inline uint uint4_to_uint(const uint4 data)
{
    return 
        (data.x <<  0) | 
        (data.y <<  8) | 
        (data.z << 16) | 
        (data.w << 24) ;
}

static inline float4 uint4_to_float4_color(uint4 color, int mode)
{
    float4 colorf = {0,0,0,1};

    switch (mode) 
    {
        // HW supported conversions
        default:
        case TEX_R8:
            colorf.x = (float) color.x / 0xFFu;
            break;
        case TEX_RG8:
            colorf.xy = convert_float2(color.xy) / 0xFFu; 
            break;
        case TEX_RGB8:
            colorf.xyz = convert_float3(color.xyz) / 0xFFu; 
            break;
        case TEX_RGBA8:
            colorf = convert_float4(color) / 0xFFu; 
            break;
        case TEX_RGB565:
            colorf.xyz = convert_float3(color.xyz) / (float3){0x1Fu, 0x3Fu, 0x1Fu}; 
            break;
        case TEX_RGBA4:
            colorf = convert_float4(color) / 0xFu;
            break;
        case TEX_RGB5_A1:
            colorf = convert_float4(color) / (float4){0x1Fu, 0x1Fu, 0x1Fu, 0x1u};
            break;
    }

    return colorf;
}

static inline float4 read_vertex_buffer(ro_vertex_buffer_t vertex_buffer, uint index) {
    float4 value;

    #ifdef DEVICE_IMAGE_ENABLED
        value = read_imagef(vertex_buffer, index);
    #else
        value = vertex_buffer[index]; 
    #endif

    return value;
}

static inline void write_vertex_buffer(wo_vertex_buffer_t vertex_buffer, uint index, float4 value) 
{ 
    #ifdef DEVICE_IMAGE_ENABLED
        write_imagef(vertex_buffer, index, value);
    #else
        vertex_buffer[index] = value;
    #endif
}

#ifdef DEVICE_POINTER_REINTERPRET_ENABLED
    static inline ro_buffer_t buffer_offset_u8 (ro_buffer_t buf, size_t offset) { return (ro_buffer_t)((global const uchar*)  buf + offset); }
    static inline ro_buffer_t buffer_offset_u16(ro_buffer_t buf, size_t offset) { return (ro_buffer_t)((global const ushort*) buf + offset); }
    static inline ro_buffer_t buffer_offset_u32(ro_buffer_t buf, size_t offset) { return (ro_buffer_t)((global const uint*)   buf + offset); }

    static inline uchar  buffer_load_u8 (ro_buffer_t buf, uint idx) { return ((global const uchar*)  buf)[idx]; }
    static inline ushort buffer_load_u16(ro_buffer_t buf, uint idx) { return ((global const ushort*) buf)[idx]; }
    static inline uint   buffer_load_u32(ro_buffer_t buf, uint idx) { return ((global const uint*)   buf)[idx]; }

    static inline void   buffer_store_u8 (rw_buffer_t buf, uint idx, uchar  v) { ((global uchar*)  buf)[idx] = v; }
    static inline void   buffer_store_u16(rw_buffer_t buf, uint idx, ushort v) { ((global ushort*) buf)[idx] = v; }
    static inline void   buffer_store_u32(rw_buffer_t buf, uint idx, uint   v) { ((global uint*)   buf)[idx] = v; }
#else
    static inline ro_buffer_t buffer_offset_u8 (ro_buffer_t buf, size_t offset) { return &buf[offset * 1]; }
    static inline ro_buffer_t buffer_offset_u16(ro_buffer_t buf, size_t offset) { return &buf[offset * 2]; }
    static inline ro_buffer_t buffer_offset_u32(ro_buffer_t buf, size_t offset) { return &buf[offset * 4]; }

    static inline uchar  buffer_load_u8 (ro_buffer_t p, uint idx) { return p[idx]; }

    static inline ushort buffer_load_u16(ro_buffer_t p, uint idx) 
    { 
        idx *= 2; 
        return 
            ((uint)p[idx + 0] << 0) | 
            ((uint)p[idx + 1] << 8) ;
    }

    static inline uint buffer_load_u32(ro_buffer_t p, uint idx) 
    { 
        idx *= 4;

        return 
            ((uint)p[idx + 0] <<  0) | 
            ((uint)p[idx + 1] <<  8) | 
            ((uint)p[idx + 2] << 16) | 
            ((uint)p[idx + 3] << 24) ;
    }

    static inline void buffer_store_u8 (rw_buffer_t p, uint idx, uchar v) { p[idx] = v; }

    static inline void buffer_store_u16(rw_buffer_t p, uint idx, ushort v)
    { 
        idx *=2; 
    
        p[idx + 0] = v; 
        p[idx + 1] = v >> 8; 
    }

    static inline void buffer_store_u32(rw_buffer_t p, uint idx, uint v)
    {
        idx *= 4;

        p[idx + 0] = v >>  0; 
        p[idx + 1] = v >>  8; 
        p[idx + 2] = v >> 16; 
        p[idx + 3] = v >> 24;
    }
#endif

static inline uint read_colorbuffer(colorbuffer_t colorbuffer, int2 pos, uint2 size, uint mode)
{

    #ifdef DEVICE_RW_IMAGE_ENABLED
    {
        return uint4_to_uint(read_imageui(colorbuffer, pos));
    }
    #else
    {
        uint offset = pos.y * size.x + pos.x;

        switch(mode) 
        {
            default:
            case TEX_R8:
                return buffer_load_u8(colorbuffer, offset);
            case TEX_RG8:
                return buffer_load_u16(colorbuffer, offset);
            case TEX_RGB8:
            {
                offset *= 3;
                return
                    ((uint) buffer_load_u8(colorbuffer, offset + 0) <<  0) |
                    ((uint) buffer_load_u8(colorbuffer, offset + 1) <<  8) |
                    ((uint) buffer_load_u8(colorbuffer, offset + 2) << 16) |
                    0xFF000000            ;
            }
            case TEX_RGBA8:
                return buffer_load_u32(colorbuffer, offset);
            case TEX_RGBA4:
            {
                uint tex = buffer_load_u16(colorbuffer, offset);
                return
                    (tex & 0x000F) << 4  |
                    (tex & 0x00F0) << 8  |
                    (tex & 0x0F00) << 12 |
                    (tex & 0xF000) << 16;
            }
            case TEX_RGB5_A1:
            {
                uint tex = buffer_load_u16(colorbuffer, offset);
                return
                    (tex & 0x001F) << 3  |
                    (tex & 0x03E0) << 6  |
                    (tex & 0x7C00) << 9  |
                    (tex & 0x8000) << 16;
            }
            case TEX_RGB565:
            {
                uint tex = buffer_load_u16(colorbuffer, offset);
                return
                    (tex & 0x001F) << 3 |
                    (tex & 0x07E0) << 5 |
                    (tex & 0xF800) << 8 |
                    0xFF000000          ;
            }
        }
    }
    #endif
}

static inline void write_2d_texture_buffer(rw_buffer_t buffer, int2 pos, uint2 size, uint mode, uint value)
{
    uint offset = pos.y * size.x + pos.x;

    switch(mode) {
        default:
        case TEX_R8:
            buffer_store_u8(buffer, offset, value);
            break;
        case TEX_RG8:
            buffer_store_u16(buffer, offset, value);
            break;
        case TEX_RGB8:
            offset *= 3;
            buffer_store_u8(buffer, offset + 0, value >>  0);
            buffer_store_u8(buffer, offset + 1, value >>  8);
            buffer_store_u8(buffer, offset + 2, value >> 16);
            break;
        case TEX_RGBA8:
            buffer_store_u32(buffer, offset, value);
            break;
        case TEX_RGBA4:
            buffer_store_u16(buffer, offset,
                (value & 0x000000F0) >> 4  |
                (value & 0x0000F000) >> 8  |
                (value & 0x00F00000) >> 12 |
                (value & 0xF0000000) >> 16);
            break;
        case TEX_RGB5_A1:
            buffer_store_u16(buffer, offset,
                (value & 0x000000F8) >> 3  |
                (value & 0x0000F800) >> 6  |
                (value & 0x00F80000) >> 9  |
                (value & 0x80000000) >> 16);
            break;
        case TEX_RGB565:
            buffer_store_u16(buffer, offset,
                (value & 0x000000F8) >> 3 |
                (value & 0x0000FC00) >> 5 |
                (value & 0x00F80000) >> 8);
            break;
    }
}

static inline void write_colorbuffer(colorbuffer_t colorbuffer, int2 pos, uint2 size, uint mode, uint color) 
{
    #ifdef DEVICE_RW_IMAGE_ENABLED
    {
        write_imageui(colorbuffer, pos, uint_to_uint4(color, mode));
    }
    #else
    {
        write_2d_texture_buffer(colorbuffer, pos, size, mode, color);
    }
    #endif
}

static inline ushort read_depthbuffer(depthbuffer_t depthbuffer, int2 pos, uint2 size)
{
    #ifdef DEVICE_RW_IMAGE_ENABLED
    {
        return read_imageui(depthbuffer, pos).x;
    }
    #else
    {
        uint offset = pos.y * size.x + pos.x;
        return depthbuffer[offset];
    }
    #endif
}

static inline void write_depthbuffer(depthbuffer_t depthbuffer, int2 pos, uint2 size, ushort depth) 
{
    #ifdef DEVICE_RW_IMAGE_ENABLED
    {
        write_imageui(depthbuffer, pos, depth);
    }
    #else
    {
        uint offset = pos.y * size.x + pos.x;
        depthbuffer[offset] = depth;
    }
    #endif
}

static inline ushort read_stencilbuffer(stencilbuffer_t stencilbuffer, int2 pos, uint2 size)
{
    #ifdef DEVICE_RW_IMAGE_ENABLED
    {
        return read_imageui(stencilbuffer, pos).x;
    }
    #else
    {
        uint offset = pos.y * size.x + pos.x;
        return stencilbuffer[offset];
    }
    #endif
}

static inline void write_stencilbuffer(stencilbuffer_t stencilbuffer, int2 pos, uint2 size, ushort stencil) 
{
    #ifdef DEVICE_RW_IMAGE_ENABLED
    {
        write_imageui(stencilbuffer, pos, stencil);
    }
    #else
    {
        uint offset = pos.y * size.x + pos.x;
        stencilbuffer[offset] = stencil;
    }
    #endif
}


#endif