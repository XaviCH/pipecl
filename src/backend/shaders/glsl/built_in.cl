#ifndef BACKEND_SHADERS_UTILS_BUILT_IN_CL
#define BACKEND_SHADERS_UTILS_BUILT_IN_CL

#ifdef __COMPILER_RELATIVE_PATH__
#include <backend/utils/texture.cl>
#include <backend/shaders/glsl/types.cl>
#include <constants.device.h>
#else
#include "glsc2/src/backend/utils/texture.cl"
#include "glsc2/src/backend/shaders/glsl/types.cl"
#include "glsc2/src/constants.device.h"
#endif

float4 mul(const float16 mat, const float4 vec)
{
    float4 result;

    result  = mat.s0123 * vec.s0;
    result += mat.s4567 * vec.s1;
    result += mat.s89ab * vec.s2;
    result += mat.scdef * vec.s3;

    return result;
}

static inline float mod(float x, float y) {
    return fmod(x,y); //x - y * floor(x/y);
}


float4 texture2D(TEXTURE_UNIT_PARAM(texture), texture_data_t texture_data, float2 coord) 
{
    float lod = 0; // TODO: mipmapping disabled, see how to enable this
    return read_2d_texturef(TEXTURE_UNIT_ARG(texture), texture_data, coord, lod);
}

#endif // BACKEND_SHADERS_UTILS_BUILT_IN_CL