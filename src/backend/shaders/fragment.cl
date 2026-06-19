#ifndef BACKEND_SHADERS_FRAGMENT_CL
#define BACKEND_SHADERS_FRAGMENT_CL

#include <backend/shaders/wrapper.cl>
#include <backend/types.cl>
#include <constants.device.h>

//---------------------------
// fragment built-in: texture sampling
//---------------------------

#define OP_TEX_CASE(coord, i) \
    case i: color = texture2D(TEXTURE_UNIT_ARG(gl_texture_unit_##i), gl_texture_datas[i], coord, coord##_grad); break;

#define TEXTURE2D(sampler, coord) ({ \
    float4 color; \
    switch (sampler) { \
        default: \
        FOR_EACH(OP_TEX_CASE, coord, IOTA(DEVICE_TEXTURE_UNITS)) \
    } \
    color; \
})

static inline float4 get_varying_at_vertex(
    int varying_idx, int vert_idx,
    ro_vertex_buffer_t vertex_buffer
) {
    size_t idx =  vert_idx * (sizeof(vertex_shader_output_t) / sizeof(float4)) + varying_idx + 1;

    return read_vertex_buffer(vertex_buffer, idx);
}

float4 interpolate_varying(
    int varying_idx, const uint3 vert_idx, const float3 bary,
    ro_vertex_buffer_t vertex_buffer
) {
    float4 v0 = get_varying_at_vertex(varying_idx, vert_idx.x, vertex_buffer);
    float4 v1 = get_varying_at_vertex(varying_idx, vert_idx.y, vertex_buffer);
    float4 v2 = get_varying_at_vertex(varying_idx, vert_idx.z, vertex_buffer);
    return v0 * bary.x + v1 * bary.y + v2 * bary.z;
}

float4 interpolate_varying_grad(
    int varying_idx, const uint3 vert_idx, const float3 bary, const float4 dbary,
    ro_vertex_buffer_t vertex_buffer, float4* out_value
) {
    float4 v0 = get_varying_at_vertex(varying_idx, vert_idx.x, vertex_buffer);
    float4 v1 = get_varying_at_vertex(varying_idx, vert_idx.y, vertex_buffer);
    float4 v2 = get_varying_at_vertex(varying_idx, vert_idx.z, vertex_buffer);
    *out_value = v0 * bary.x + v1 * bary.y + v2 * bary.z;

    float2 e1 = (v1 - v0).xy;
    float2 e2 = (v2 - v0).xy;
    float2 ddx = e1 * dbary.x + e2 * dbary.z;
    float2 ddy = e1 * dbary.y + e2 * dbary.w;
    return (float4){ddx.x, ddx.y, ddy.x, ddy.y};
}

#define OP_SVG2(ctx, a) { \
    float4 __vv; \
    a##_grad = interpolate_varying_grad(varying++, vert_idx, bary, gl_dbary, vertex_buffer, &__vv); \
    a = __vv.xy; \
    }

#define SET_VARYING_VEC2_GRAD(...) FOR_EACH(OP_SVG2, ~, __VA_ARGS__)

#define OP_GD2(ctx, a) float4 a##_grad;
#define DEFINE_VARYING_VEC2_GRAD(...) FOR_EACH(OP_GD2, ~, __VA_ARGS__)

//---------------------------
// Varying interpolation setters (FS-only)
//---------------------------

#define OP_SETVARYING(ctx, a) assign_varying(&a, interpolate_varying(varying++, vert_idx, bary, vertex_buffer));
#define SET_VARYING_CHAIN(...) FOR_EACH(OP_SETVARYING, ~, __VA_ARGS__)

//------------------------------------
// Fragment-stage uniform kernel params
//------------------------------------

#ifdef FS_UNIFORM_FLOAT
    #define FS_KERNEL_PARAM_UNIFORM_FLOAT COMMA_CHAIN(global float*, FS_UNIFORM_FLOAT)
#else
    #define FS_KERNEL_PARAM_UNIFORM_FLOAT
#endif

#ifdef FS_UNIFORM_INT
    #define FS_KERNEL_PARAM_UNIFORM_INT COMMA_CHAIN(global int*, FS_UNIFORM_INT)
#else
    #define FS_KERNEL_PARAM_UNIFORM_INT
#endif

#ifdef FS_UNIFORM_INT2
    #define FS_KERNEL_PARAM_UNIFORM_INT2 COMMA_CHAIN(global int2*, FS_UNIFORM_INT2)
#else
    #define FS_KERNEL_PARAM_UNIFORM_INT2
#endif

#ifdef FS_UNIFORM_VEC4
    #define FS_KERNEL_PARAM_UNIFORM_VEC4 COMMA_CHAIN(global float4*, FS_UNIFORM_VEC4)
#else
    #define FS_KERNEL_PARAM_UNIFORM_VEC4
#endif

#ifdef FS_UNIFORM_MAT4
    #define FS_KERNEL_PARAM_UNIFORM_MAT4 COMMA_CHAIN(global float16*, FS_UNIFORM_MAT4)
#else
    #define FS_KERNEL_PARAM_UNIFORM_MAT4
#endif

#ifdef FS_UNIFORM_SAMPLER2D
    #define FS_KERNEL_PARAM_UNIFORM_IMAGE2D COMMA_CHAIN(image2D_t, PREPEND_CHAIN(gl_image2D_, FS_UNIFORM_SAMPLER2D))
    #define FS_KERNEL_PARAM_UNIFORM_SAMPLER2D COMMA_CHAIN(sampler2D_t, FS_UNIFORM_SAMPLER2D)
    #define FS_KERNEL_ARG_UNIFORM_IMAGE2D COMMA_CHAIN(,PREPEND_CHAIN(gl_image2D_,FS_UNIFORM_SAMPLER2D))
#else
    #define FS_KERNEL_PARAM_UNIFORM_IMAGE2D
    #define FS_KERNEL_PARAM_UNIFORM_SAMPLER2D
    #define FS_KERNEL_ARG_UNIFORM_IMAGE2D
#endif

#define FS_KERNEL_PARAM_UNIFORMS \
    FS_KERNEL_PARAM_UNIFORM_MAT4 \
    FS_KERNEL_PARAM_UNIFORM_VEC4 \
    FS_KERNEL_PARAM_UNIFORM_INT2 \
    FS_KERNEL_PARAM_UNIFORM_SAMPLER2D \
    FS_KERNEL_PARAM_UNIFORM_INT \
    FS_KERNEL_PARAM_UNIFORM_FLOAT



//---------------------------
// Texture-unit kernel wiring
//---------------------------

#define OP_TEX_ARG(_S, i)   TEXTURE_UNIT_ARG(_S##_##i)
#define OP_TEX_PARAM(_S, i) TEXTURE_UNIT_PARAM(_S##_##i)
#define TA_REPEAT(_S) FOR_EACH_C(OP_TEX_ARG,   _S, IOTA(DEVICE_TEXTURE_UNITS))
#define TP_REPEAT(_S) FOR_EACH_C(OP_TEX_PARAM, _S, IOTA(DEVICE_TEXTURE_UNITS))

#if DEVICE_TEXTURE_UNITS < 1 || DEVICE_TEXTURE_UNITS > 16
    #error DEVICE_TEXTURE_UNITS must be in [1, 16] (preprocessor IOTA/COUNT limit)
#endif

// wrapper for fragment shader main args, OpenCL doesn't support structs for kernel args (e.g. image2D_t)
#define fragment_shader_kernel_args \
    TA_REPEAT(gl_texture_unit), \
    gl_texture_datas

#define fragment_shader_kernel_params \
    TP_REPEAT(gl_texture_unit), \
    global const texture_data_t* gl_texture_datas

typedef struct {
    float4 gl_FragColor;
    float gl_FragDepth;
} fragment_shader_output_t;


//---------------------------------
// fragment shader main definitions
//---------------------------------

#define FS_DEFINE_GRADS \
    FS_DEFINE_VARYING_VEC2_GRAD

#define FS_DEFINES \
    float4 gl_FragColor; \
    DEFINE_VARYINGS \
    DEFINE_UNIFORMS \
    FS_DEFINE_GRADS

#define FS_SETS \
    SET_VARYINGS \
    SET_UNIFORMS

#define FS_MAIN(...) \
    inline bool gl_fragment_shader( \
        uniform_addr_space uniform_buffer_t* restrict gl_uniforms, \
        fragment_shader_kernel_params, \
        fragment_shader_output_t* output, \
        ro_vertex_buffer_t vertex_buffer, \
        uint3 vert_idx, float3 bary, float4 gl_dbary \
    ) { \
        /* Define accessible objects from fragment shader */ \
        FS_DEFINES \
        /* Set values from vertex_buffer */ \
        FS_SETS \
        /* Run vertex shader */ \
        __VA_ARGS__ \
        /* Fill fragment shader output */ \
        output->gl_FragColor = gl_FragColor; \
        return true; \
    }

//-----------------------------
// data reflection for frontend
//-----------------------------

kernel void gl_fs_uniform_data(
    FS_KERNEL_PARAM_UNIFORMS
    private int test
) {
    return;
}

#endif // BACKEND_SHADERS_FRAGMENT_CL
