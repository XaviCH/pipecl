#ifndef BACKEND_UTILS_TEXTURE_CL 
#define BACKEND_UTILS_TEXTURE_CL

#include <backend/types.cl>

// VARIABLE wrappers

#define TEXTURE_UNIT_IMAGE_NAME(_NAME) _NAME##_image
#define TEXTURE_UNIT_SAMPLER_NAME(_NAME) _NAME##_sampler

#ifdef DEVICE_IMAGE_ENABLED
    #define TEXTURE_UNIT_PARAM(_NAME) \
        ro_texture2d_t TEXTURE_UNIT_IMAGE_NAME(_NAME), \
        sampler_t TEXTURE_UNIT_SAMPLER_NAME(_NAME)
#else
    #define TEXTURE_UNIT_PARAM(_NAME) \
        ro_texture2d_t TEXTURE_UNIT_IMAGE_NAME(_NAME)
#endif

#ifdef DEVICE_IMAGE_ENABLED
    #define TEXTURE_UNIT_ARG(_NAME) \
        TEXTURE_UNIT_IMAGE_NAME(_NAME), \
        TEXTURE_UNIT_SAMPLER_NAME(_NAME)
#else
    #define TEXTURE_UNIT_ARG(_NAME) \
        TEXTURE_UNIT_IMAGE_NAME(_NAME)
#endif

// GET functions

/**
 * @pre lod is a valid value
 */
static inline uint get_lod_offset(texture_size_t size, uint lod);

/**
 * @pre lod is a valid value
 */
static inline texture_size_t get_lod_size(texture_size_t size, uint lod);

/**
 * @pre lod is a valid value
 * @param ncoord coords must range [0,1]
 */
static inline float2 get_lod_ucoordf(float2 ncoord, texture_size_t size, uint lod);

/**
 */
static inline float2 get_wrapped_ncoord(float2 coord, const texture_data_t texture_data);

// READ functions

/**
 * @param lod will access
 */
static inline float4 read_2d_bufferf(
    global const void* buffer, 
    const texture_data_t texture_data,
    float2 coord,
    float lod
);

/**
 * @param coord
 */
static inline uint4 read_2d_bufferui(
    global const void* buffer,
    const texture_data_t texture_data,
    int2 coord
);

/**
 */
static inline float4 read_2d_texturef(
    TEXTURE_UNIT_PARAM(texture),
    const texture_data_t texture_data,
    float2 coord,
    float lod
);

/**
 * @param coord
 */
static inline uint4 read_2d_textureui(
    ro_texture2d_t texture,
    int2 coord,
    const texture_data_t texture_data
);

/**
 * This function catches exceptions for non supporting mappings in OpenCL.
 * Using this function without enabling device image support will return black color.
 */
static inline float4 read_imagef_gl(
    TEXTURE_UNIT_PARAM(texture),
    const texture_data_t texture_data,
    float2 coord,
    float lod
);

// ------------------------------------------------------------------------------
// Implementation
// ------------------------------------------------------------------------------

static inline uint get_lod_offset(texture_size_t size, uint lod)
{
    uint offset = 0;

    for(uint level = 0; level < lod; ++level)
    {
        offset += size.x + size.y;
        size /= (texture_size_t)2;
    }

    return offset;
}

static inline texture_size_t get_lod_size(texture_size_t size, uint lod)
{
    for(uint level = 0; level < lod; ++level)
    {
        size /= (texture_size_t)2;
    }
    
    return max(size,(texture_size_t){1,1});
}


static inline float2 get_lod_ucoordf(float2 ncoord, texture_size_t size, uint lod)
{
    return ncoord * convert_float2(get_lod_size(size, lod));
}

static inline float2 get_wrapped_ncoord(float2 coord, const texture_data_t texture_data)
{
    float2  ncoord;

    if (get_texture_data_require_clamp_x(texture_data))
    {
        ncoord.x = clamp(coord.x, 0.f,1.f);
    }
    else 
    {
        float tmp;
        ncoord.x = fract(coord.x, &tmp);
    }

    if (get_texture_data_require_clamp_y(texture_data)) 
    {
        ncoord.y = clamp(coord.y, 0.f,1.f);
    }
    else
    {
        float tmp;
        ncoord.y = fract(coord.y, &tmp);
    }

    if (get_texture_data_require_negate_x(texture_data, coord.x)) ncoord.x = -ncoord.x;
    if (get_texture_data_require_negate_y(texture_data, coord.y)) ncoord.y = -ncoord.y;

    if (get_texture_data_require_diff_x(texture_data, coord.x)) ncoord.x = 1.0f - ncoord.x;
    if (get_texture_data_require_diff_y(texture_data, coord.y)) ncoord.y = 1.0f - ncoord.y;

    return clamp(ncoord,0.f,1.f);
}

// ------------------------------------------------------------------------------

static inline float4 read_2d_bufferf(
    global const void* buffer, 
    const texture_data_t texture_data,
    float2 coord,
    float lod
) {

    // retrieve image configuration
    texture_size_t  size    = get_texture_data_size(texture_data);
    int             mode    = get_texture_data_mode(texture_data);
    bool            linear  = is_texture_data_linear(texture_data);
    int             csize   = get_texture_data_color_size(texture_data);

    // generate wrap ncoord
    float2  ncoord = get_wrapped_ncoord(coord, texture_data);

    // generate mipmapping objects
    uint                lod_offset  = get_lod_offset(size, lod) * csize;
    texture_size_t      lod_size    = get_lod_size(size, lod);
    global const void*  lod_buffer  = (global const void*) ((global const uchar*) buffer + lod_offset);
    float2              lod_ucoord  = get_lod_ucoordf(ncoord, size, lod);
    

    // generate image coordanates
    int2 floor_coord = convert_int2_rtz(lod_ucoord);
    int2 ceil_coord = min(convert_int2_rtp(lod_ucoord), convert_int2(size));

    float2 frac_coord   = lod_ucoord - floor(lod_ucoord);

    // apply GL wrap and filtering
    float4 color = {0,0,0,0};
    
    for(uint x=0; x<2; ++x) for(uint y=0; y<2; ++y)
    {
        int2 icoord = {
            x ? ceil_coord.x : floor_coord.x,
            y ? ceil_coord.y : floor_coord.y
        };

        uint4 colorui = read_2d_bufferui(lod_buffer, texture_data, icoord);
        
        if (! linear) return uint4_to_float4_color(colorui, mode);

        float2 scale = {
            x ? frac_coord.x : 1.0f - frac_coord.x,
            y ? frac_coord.y : 1.0f - frac_coord.y
        };
        
        color += uint4_to_float4_color(colorui, mode) * scale.x * scale.y;
    }

    return color;
}

static inline uint4 read_2d_bufferui(
    global const void* buffer,
    const texture_data_t texture_data,
    int2 coord
) {
    uint4 color;

    texture_size_t   size = get_texture_data_size(texture_data);
    int     mode = get_texture_data_mode(texture_data);

    uint offset = coord.y * size.x + coord.x;

    switch(mode) 
    {
        default:
        case TEX_R8:
        {
            global const uchar* ptr = (global const uchar*) buffer + offset;
            color.x = ptr[0];
            break;
        }
        case TEX_RG8:
        {
            global const uchar* ptr = (global const uchar*) buffer + offset*2;
            color.x = ptr[0]; 
            color.y = ptr[1];
            break;
        }
        case TEX_RGB8:
        {
            global const uchar* ptr = (global const uchar*) buffer + offset*3;
            color.x = ptr[0];
            color.y = ptr[1];
            color.z = ptr[2];
            break;
        }
        case TEX_RGBA8:
        {
            global const uchar* ptr = (global const uchar*) buffer + offset*4;
            color.x = ptr[0];
            color.y = ptr[1];
            color.z = ptr[2];
            color.w = ptr[3];
            break;
        }
        case TEX_RGBA4:
        {
            ushort tex = ((global const ushort*) buffer)[offset];
            color.x = (tex >>  0) & 0xFu;
            color.y = (tex >>  4) & 0xFu;
            color.z = (tex >>  8) & 0xFu;
            color.w = (tex >> 12) & 0xFu;
            break;
        }
        case TEX_RGB5_A1:
        {
            ushort tex = ((global const ushort*) buffer)[offset];
            color.x = (tex >>  0) & 0x1Fu;
            color.y = (tex >>  5) & 0x1Fu;
            color.z = (tex >> 10) & 0x1Fu;
            color.w = (tex >> 15) & 0x1u;
            break;
        }
        case TEX_RGB565:
        {
            ushort tex = ((global const ushort*) buffer)[offset];
            color.x = (tex >>  0) & 0x1Fu;
            color.y = (tex >>  5) & 0x3Fu;
            color.z = (tex >> 11) & 0x1Fu;
            break;
        }
    }

    return color;
}

static inline float4 read_2d_texturef(
    TEXTURE_UNIT_PARAM(texture),
    const texture_data_t texture_data,
    float2 coord,
    float lod
) {
    float4 color;

    int max_level = get_texture_data_max_level(texture_data);
    bool mipmapping = is_texture_data_mipmapping(texture_data);
    bool mipmapping_linear = is_texture_data_mipmapping_linear(texture_data);

    if (! mipmapping) 
    {
        #ifdef DEVICE_IMAGE_ENABLED
        {
            return read_imagef_gl(TEXTURE_UNIT_ARG(texture), texture_data, coord, 0.f);
        }
        #else
            return read_2d_bufferf(TEXTURE_UNIT_ARG(texture), texture_data, coord, 0.f);
        #endif
    }

    if (! mipmapping_linear)
    {
        #ifdef DEVICE_IMAGE_ENABLED
            return read_imagef_gl(TEXTURE_UNIT_ARG(texture), texture_data, coord, round(lod));
        #else
            return read_2d_bufferf(TEXTURE_UNIT_ARG(texture), texture_data, coord, round(lod));
        #endif
    }

    float lod1 = floor(lod);
    float lod2 = lod1 + 1;
    float lod_frac = lod - floor(lod); // TODO: checkout to fract

    float4 color_0, color_1;

    #ifdef DEVICE_IMAGE_ENABLED
    {
        color_0 = read_imagef_gl(TEXTURE_UNIT_ARG(texture), texture_data, coord, lod1);
        color_1 = read_imagef_gl(TEXTURE_UNIT_ARG(texture), texture_data, coord, lod2);
    }
    #else
    {
        color_0 = read_2d_bufferf(TEXTURE_UNIT_ARG(texture), texture_data, coord, lod1);
        color_1 = read_2d_bufferf(TEXTURE_UNIT_ARG(texture), texture_data, coord, lod2);
    }
    #endif
    
    return color_0 * (1- lod_frac) + color_1 * lod_frac;
}

static inline uint4 read_2d_textureui(
    ro_texture2d_t texture,
    int2 coord,
    const texture_data_t texture_data
) {
    uint4 color;

    #ifdef DEVICE_IMAGE_ENABLED
    {
        color = read_imageui(texture, coord);
    }
    #else
    {
        color = read_2d_bufferui(texture, texture_data, coord);
    }
    #endif

    return color;
}

static inline float4 read_imagef_gl(
    TEXTURE_UNIT_PARAM(texture),
    const texture_data_t texture_data,
    float2 coord,
    float lod
) {
    #ifdef DEVICE_IMAGE_ENABLED

    if (get_texture_data_require_clamp_x(texture_data))
    {
        coord.x = clamp(coord.x, 0.f,1.f);
    }

    if (get_texture_data_require_clamp_y(texture_data))
    {
        coord.y = clamp(coord.y, 0.f,1.f);
    }
    
    if (get_texture_data_require_negate_x(texture_data, coord.x))
    {
        coord.x = -coord.x;
    }

    if (get_texture_data_require_negate_y(texture_data, coord.y))
    {
        coord.y = -coord.y;
    }

    bool software_support = get_texture_data_require_software_support(texture_data);

    if (! software_support)
    {
        return read_imagef(TEXTURE_UNIT_ARG(texture), coord, lod);
    }

    int mode = get_texture_data_mode(texture_data);
    
    return uint4_to_float4_color(read_imageui(TEXTURE_UNIT_ARG(texture), coord, lod), mode);

    #endif

    return (float4){0,0,0,1};
}

#endif // BACKEND_UTILS_TEXTURE_CL