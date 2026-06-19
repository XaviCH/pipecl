#ifndef BACKEND_SHADERS_UTILS_BUILT_IN_CL
#define BACKEND_SHADERS_UTILS_BUILT_IN_CL

#include <backend/utils/texture.cl>
#include <backend/shaders/glsl/types.cl>
#include <constants.device.h>

float4 mul(const float16 mat, const float4 vec)
{
    float4 result;

    result  = mat.s0123 * vec.s0;
    result += mat.s4567 * vec.s1;
    result += mat.s89ab * vec.s2;
    result += mat.scdef * vec.s3;

    return result;
}

float mod(float x, float y) {
    return fmod(x,y); //x - y * floor(x/y);
}

static inline float compute_lod(texture_data_t texture_data, float4 grad)
{
    float2 texsize = convert_float2(get_texture_data_size(texture_data));
    float2 ddx = grad.xy * texsize;
    float2 ddy = grad.zw * texsize;
    float rho2 = fmax(dot(ddx, ddx), dot(ddy, ddy));
    float lod = 0.5f * log2(fmax(rho2, 1e-20f));
    return clamp(lod, 0.0f, (float)get_texture_data_max_level(texture_data));
}

float4 texture2D(TEXTURE_UNIT_PARAM(texture), texture_data_t texture_data, float2 coord, float4 grad)
{
    float lod = compute_lod(texture_data, grad);
    return read_2d_texturef(TEXTURE_UNIT_ARG(texture), texture_data, coord, lod);
}

#endif // BACKEND_SHADERS_UTILS_BUILT_IN_CL