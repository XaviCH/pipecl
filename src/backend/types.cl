#ifndef BACKEND_TYPES_CL
#define BACKEND_TYPES_CL

#ifdef __COMPILER_RELATIVE_PATH__
#include <types.device.h>
#else
#include "glsc2/src/types.device.h"
#endif

#ifdef DEVICE_IMAGE_ENABLED
    typedef read_only image1d_buffer_t ro_vertex_buffer_t;
    typedef write_only image1d_buffer_t wo_vertex_buffer_t;
    typedef read_only image2d_t ro_texture2d_t;
#else
    typedef global const float4* restrict ro_vertex_buffer_t;
    typedef global float4* restrict wo_vertex_buffer_t;
    typedef global const void* restrict ro_texture2d_t;
#endif

#if DEVICE_RW_IMAGE_ENABLED
    typedef read_write image2d_t rw_texture2d_t;
    typedef read_write image2d_t colorbuffer_t;
    typedef read_write image2d_t depthbuffer_t;
    typedef read_write image2d_t stencilbuffer_t;
#else
    typedef global void* restrict rw_texture2d_t;
    typedef global void* restrict colorbuffer_t;
    typedef global ushort* restrict depthbuffer_t;
    typedef global uchar* restrict stencilbuffer_t;
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

        // SW supported conversions
        case TEX_RGBA4:
            {
                uint4 tmp = 
                {
                    (color.x >> 0) & 0xFu,
                    (color.x >> 4) & 0xFu,
                    (color.y >> 0) & 0xFu,
                    (color.y >> 4) & 0xFu,
                };
                colorf = convert_float4(tmp) / 0xFu; 
            }
            break;
        case TEX_RGB5_A1:
            {
                uint4 tmp = 
                {
                    (color.x >>  0) & 0x1Fu,
                    (color.x >>  5) & 0x1Fu,
                    (color.x >> 10) & 0x1Fu,
                    (color.x >> 15) & 0x1u,
                };
                colorf = convert_float4(tmp) / (float4){0x1Fu, 0x1Fu, 0x1Fu, 0x1u}; 
            }
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
                return *((global const uchar*) colorbuffer + offset);
            case TEX_RG8:
                return *((global const ushort*) colorbuffer + offset);
            case TEX_RGB8:
            {
                global const uchar* ptr = (global const uchar*) colorbuffer + offset*3;
                return 
                    ((uint) ptr[0] <<  0) |
                    ((uint) ptr[1] <<  8) |
                    ((uint) ptr[2] << 16) |
                    0xFF000000            ;
            }
            case TEX_RGBA8:
                return *((global const uint*) colorbuffer + offset);
            case TEX_RGBA4:
            {
                uint tex = *((global const ushort*) colorbuffer + offset);
                return 
                    (tex & 0x000F) << 0 |
                    (tex & 0x00F0) << 4 |
                    (tex & 0x0F00) << 8 |
                    (tex & 0xF000) << 12;
            }
            case TEX_RGB5_A1:
            {
                uint tex = *((global const ushort*) colorbuffer + offset);
                return 
                    (tex & 0x001F) << 0 |
                    (tex & 0x03E0) << 5 |
                    (tex & 0x7C00) << 10|
                    (tex & 0x8000) << 15;
            }
            case TEX_RGB565:
            {
                uint tex = *((global const ushort*) colorbuffer + offset);
                return 
                    (tex & 0x001F) << 0 |
                    (tex & 0x07E0) << 5 |
                    (tex & 0xF800) << 11;
            }
        }
    }
    #endif
}

static inline void write_2d_texture_buffer(global void* buffer, int2 pos, uint2 size, uint mode, uint value)
{
    uint offset = pos.y * size.x + pos.x;

    switch(mode) {
        default:
        case TEX_R8:
            *((global uchar*) buffer + offset) = value;
            break;
        case TEX_RG8:
            *((global ushort*) buffer + offset) = value;
            break;
        case TEX_RGB8:
            {
                global uchar* buffer = (global uchar*) buffer + (offset*3);
                buffer[0] = value >>  0;
                buffer[1] = value >>  8;
                buffer[2] = value >> 16;
            }
            break;
        case TEX_RGBA8:
            *(((global uint*) buffer) + offset) = value;
            break;
        case TEX_RGBA4:
            *((global ushort*) buffer + offset) = 
                (value & 0x0000000F) >> 0 |
                (value & 0x00000F00) >> 4 |
                (value & 0x000F0000) >> 8 |
                (value & 0x0F000000) >> 12;
            break;
        case TEX_RGB5_A1:
            *((global ushort*) buffer + offset) = 
                (value & 0x0000001F) >> 0 |
                (value & 0x00001F00) >> 3 |
                (value & 0x001F0000) >> 6 |
                (value & 0x01000000) >> 9 ;
            break;
        case TEX_RGB565:
            *((global ushort*) buffer + offset) = 
                (value & 0x0000001F) >> 0 |
                (value & 0x00003F00) >> 3 |
                (value & 0x001F0000) >> 5 ;
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