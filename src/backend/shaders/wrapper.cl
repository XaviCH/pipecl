/**
 * OpenCL shader wrapper
 * 
 * Wrapper for OpenCL shaders to provide a consistent interface
 * and utility functions.
 *
 * Vertex and fragment shaders can use this wrapper to access
 * built-in functions, uniform variables, and varying data. 
 */

#ifndef BACKEND_SHADERS_WRAPPER_CL
#define BACKEND_SHADERS_WRAPPER_CL

#ifdef __COMPILER_RELATIVE_PATH__
    #include <backend/shaders/glsl/built_in.cl>
    #include <backend/shaders/glsl/macros.h>
    #include <constants.device.h>
#else
    #include "glsc2/src/backend/shaders/glsl/built_in.cl"
    #include "glsc2/src/backend/shaders/glsl/macros.h"
    #include "glsc2/src/constants.device.h"
#endif

//---------------------------
// Built-in wrapper functions
//---------------------------

#define CASE_TEXTURE2D(color, sampler, coord) \
    case sampler: color = texture2D(TEXTURE_UNIT_ARG(gl_texture_unit_##sampler), gl_texture_datas[sampler], coord); break;

#define TEXTURE2D(sampler, coord) ({ \
    float4 color; \
    switch (sampler) { \
        default: \
        CASE_TEXTURE2D(color, 0, coord); \
        CASE_TEXTURE2D(color, 1, coord); \
        CASE_TEXTURE2D(color, 2, coord); \
        CASE_TEXTURE2D(color, 3, coord); \
        CASE_TEXTURE2D(color, 4, coord); \
        CASE_TEXTURE2D(color, 5, coord); \
        CASE_TEXTURE2D(color, 6, coord); \
        CASE_TEXTURE2D(color, 7, coord); \
    } \
    color; \
})
/*
#define TEXTURE2D(sampler, coord) \
    texture2D(gl_texture_datas[0], gl_texture_unit_0, coord);
*/
//--------------------
// Varying definitions
//--------------------

#ifdef VARYING_FLOAT
    #define PARAM_VARYING_FLOAT COMMA_CHAIN(float, VARYING_FLOAT)
    #define DEFINE_VARYING_FLOAT STRUCT_CHAIN(float, VARYING_FLOAT)
    #define OUTPUT_VARYING_FLOAT STRUCT_CHAIN(float4, VARYING_FLOAT)
    #define SET_STRUCT_VARYING_FLOAT SET_STRUCT_F4_F1_CHAIN(output, 0, 0, 1, VARYING_FLOAT)
    #define SET_VARYING_FLOAT SET_VARYING_F1_CHAIN(VARYING_FLOAT)
#else
    #define PARAM_VARYING_FLOAT
    #define DEFINE_VARYING_FLOAT
    #define OUTPUT_VARYING_FLOAT
    #define SET_STRUCT_VARYING_FLOAT
    #define SET_VARYING_FLOAT
#endif

#ifdef VARYING_VEC2
    #define PARAM_VARYING_VEC2 COMMA_CHAIN(float2, VARYING_VEC2)
    #define DEFINE_VARYING_VEC2 STRUCT_CHAIN(float2, VARYING_VEC2)
    #define OUTPUT_VARYING_VEC2 STRUCT_CHAIN(float4, VARYING_VEC2)
    #define SET_STRUCT_VARYING_VEC2 SET_STRUCT_F4_F2_CHAIN(output, 0, 1, VARYING_VEC2)
    #define SET_VARYING_VEC2 SET_VARYING_F2_CHAIN(VARYING_VEC2)
#else
    #define PARAM_VARYING_VEC2
    #define DEFINE_VARYING_VEC2
    #define OUTPUT_VARYING_VEC2
    #define SET_STRUCT_VARYING_VEC2
    #define SET_VARYING_VEC2
#endif

#ifdef VARYING_VEC3
    #define PARAM_VARYING_VEC3 COMMA_CHAIN(float3, VARYING_VEC3)
    #define DEFINE_VARYING_VEC3 STRUCT_CHAIN(float3, VARYING_VEC3)
    #define OUTPUT_VARYING_VEC3 STRUCT_CHAIN(float4, VARYING_VEC3)
    #define SET_STRUCT_VARYING_VEC3 SET_STRUCT_F4_F3_CHAIN(output, 1, VARYING_VEC3)
    #define SET_VARYING_VEC3 SET_VARYING_F3_CHAIN(VARYING_VEC3)
#else
    #define PARAM_VARYING_VEC3
    #define DEFINE_VARYING_VEC3
    #define OUTPUT_VARYING_VEC3
    #define SET_STRUCT_VARYING_VEC3
    #define SET_VARYING_VEC3
#endif

#ifdef VARYING_VEC4
    #define PARAM_VARYING_VEC4 COMMA_CHAIN(float3, VARYING_VEC4)
    #define DEFINE_VARYING_VEC4 STRUCT_CHAIN(float4, VARYING_VEC4)
    #define OUTPUT_VARYING_VEC4 STRUCT_CHAIN(float4, VARYING_VEC4)
    #define SET_STRUCT_VARYING_VEC4 SET_STRUCT_CHAIN(output, VARYING_VEC4)
    #define SET_VARYING_VEC4 SET_VARYING_CHAIN(VARYING_VEC4)
#else
    #define PARAM_VARYING_VEC4
    #define DEFINE_VARYING_VEC4
    #define OUTPUT_VARYING_VEC4
    #define SET_STRUCT_VARYING_VEC4
    #define SET_VARYING_VEC4
#endif

#define DEFINE_VARYINGS \
    DEFINE_VARYING_FLOAT \
    DEFINE_VARYING_VEC2 \
    DEFINE_VARYING_VEC3 \
    DEFINE_VARYING_VEC4

#define OUTPUT_VARYINGS \
    OUTPUT_VARYING_FLOAT \
    OUTPUT_VARYING_VEC2 \
    OUTPUT_VARYING_VEC3 \
    OUTPUT_VARYING_VEC4

#define PARAM_VARYINGS \
    PARAM_VARYING_FLOAT \
    PARAM_VARYING_VEC2 \
    PARAM_VARYING_VEC3 \
    PARAM_VARYING_VEC4

#define SET_STRUCT_VARYINGS \
    SET_STRUCT_VARYING_FLOAT \
    SET_STRUCT_VARYING_VEC2 \
    SET_STRUCT_VARYING_VEC3 \
    SET_STRUCT_VARYING_VEC4

#define SET_VARYINGS \
    { \
        uint varying = 0; \
        SET_VARYING_FLOAT \
        SET_VARYING_VEC2 \
        SET_VARYING_VEC3 \
        SET_VARYING_VEC4 \
    }

//----------------------
// Attribute definitions
//----------------------

#ifdef ATTRIBUTE_VEC2
    #define KERNEL_ARG_ATTRIBUTE_VEC2 COMMA_CHAIN(global const void*, JOIN_CHAIN(_,ATTRIBUTE_VEC2))
    #define DEFINE_ATTRIBUTE_VEC2 STRUCT_CHAIN(float2, ATTRIBUTE_VEC2)
    #define SET_ATTRIBUTE_VEC2 SET_ATTRIBUTE_CHAIN(ATTRIBUTE_VEC2)
#else
    #define KERNEL_ARG_ATTRIBUTE_VEC2
    #define DEFINE_ATTRIBUTE_VEC2
    #define SET_ATTRIBUTE_VEC2
#endif

#ifdef ATTRIBUTE_VEC3
    #define KERNEL_ARG_ATTRIBUTE_VEC3 COMMA_CHAIN(global const void*, JOIN_CHAIN(_,ATTRIBUTE_VEC3))
    #define DEFINE_ATTRIBUTE_VEC3 STRUCT_CHAIN(float3, ATTRIBUTE_VEC3)
    #define SET_ATTRIBUTE_VEC3 SET_ATTRIBUTE_CHAIN(ATTRIBUTE_VEC3)
#else
    #define KERNEL_ARG_ATTRIBUTE_VEC3
    #define DEFINE_ATTRIBUTE_VEC3
    #define SET_ATTRIBUTE_VEC3
#endif

#ifdef ATTRIBUTE_VEC4
    #define KERNEL_ARG_ATTRIBUTE_VEC4 COMMA_CHAIN(global const void*, JOIN_CHAIN(_,ATTRIBUTE_VEC4))
    #define DEFINE_ATTRIBUTE_VEC4 STRUCT_CHAIN(float4, ATTRIBUTE_VEC4)
    #define SET_ATTRIBUTE_VEC4 SET_ATTRIBUTE_CHAIN(ATTRIBUTE_VEC4)
#else
    #define KERNEL_ARG_ATTRIBUTE_VEC4
    #define DEFINE_ATTRIBUTE_VEC4
    #define SET_ATTRIBUTE_VEC4
#endif

#define DEFINE_ATTRIBUTES \
    DEFINE_ATTRIBUTE_VEC2 \
    DEFINE_ATTRIBUTE_VEC3 \
    DEFINE_ATTRIBUTE_VEC4

#define KERNEL_PARAM_ATTRIBUTES \
    KERNEL_ARG_ATTRIBUTE_VEC2 \
    KERNEL_ARG_ATTRIBUTE_VEC3 \
    KERNEL_ARG_ATTRIBUTE_VEC4

#define SET_ATTRIBUTES \
    { \
        uint gl_attribute_location = 0; \
        SET_ATTRIBUTE_VEC2 \
        SET_ATTRIBUTE_VEC3 \
        SET_ATTRIBUTE_VEC4 \
    }


//--------------------
// Uniform definitions
//--------------------

#define GEN_FETCH_UNIFORM_IMPL(_TYPE) \
    static inline void __attribute__((overloadable)) fetch_uniform( \
        global void* gl_uniforms, \
        private _TYPE* dst, \
        private uint* offset \
    ) { \
        *dst = *(global _TYPE*) &((global uchar*)gl_uniforms)[*offset]; \
        *offset += sizeof(_TYPE); \
    }

GEN_FETCH_UNIFORM_IMPL(float)
GEN_FETCH_UNIFORM_IMPL(float2)
GEN_FETCH_UNIFORM_IMPL(float3)
GEN_FETCH_UNIFORM_IMPL(float4)
GEN_FETCH_UNIFORM_IMPL(float16)

GEN_FETCH_UNIFORM_IMPL(int)
GEN_FETCH_UNIFORM_IMPL(int2)
GEN_FETCH_UNIFORM_IMPL(int3)
GEN_FETCH_UNIFORM_IMPL(int4)
GEN_FETCH_UNIFORM_IMPL(int16)

GEN_FETCH_UNIFORM_IMPL(uint)
GEN_FETCH_UNIFORM_IMPL(uint2)
GEN_FETCH_UNIFORM_IMPL(uint3)
GEN_FETCH_UNIFORM_IMPL(uint4)
GEN_FETCH_UNIFORM_IMPL(uint16)

#undef GEN_FETCH_UNIFORM_IMPL

#define SET_UNIFORM_CHAIN(...) GLUE(SET_UNIFORM_CHAIN_,COUNT(__VA_ARGS__))(__VA_ARGS__)
#define SET_UNIFORM_CHAIN_1(a)     fetch_uniform(gl_uniforms, &a, &gl_uniform_offset);
#define SET_UNIFORM_CHAIN_2(a,...) fetch_uniform(gl_uniforms, &a, &gl_uniform_offset); SET_UNIFORM_CHAIN_1(__VA_ARGS__)
#define SET_UNIFORM_CHAIN_3(a,...) fetch_uniform(gl_uniforms, &a, &gl_uniform_offset); SET_UNIFORM_CHAIN_2(__VA_ARGS__)
#define SET_UNIFORM_CHAIN_4(a,...) fetch_uniform(gl_uniforms, &a, &gl_uniform_offset); SET_UNIFORM_CHAIN_3(__VA_ARGS__)
#define SET_UNIFORM_CHAIN_5(a,...) fetch_uniform(gl_uniforms, &a, &gl_uniform_offset); SET_UNIFORM_CHAIN_4(__VA_ARGS__)
#define SET_UNIFORM_CHAIN_6(a,...) fetch_uniform(gl_uniforms, &a, &gl_uniform_offset); SET_UNIFORM_CHAIN_5(__VA_ARGS__)
#define SET_UNIFORM_CHAIN_7(a,...) fetch_uniform(gl_uniforms, &a, &gl_uniform_offset); SET_UNIFORM_CHAIN_6(__VA_ARGS__)
#define SET_UNIFORM_CHAIN_8(a,...) fetch_uniform(gl_uniforms, &a, &gl_uniform_offset); SET_UNIFORM_CHAIN_7(__VA_ARGS__)
#define SET_UNIFORM_CHAIN_9(a,...) fetch_uniform(gl_uniforms, &a, &gl_uniform_offset); SET_UNIFORM_CHAIN_8(__VA_ARGS__)

// ---------------------

#ifdef UNIFORM_FLOAT
    #define KERNEL_PARAM_UNIFORM_FLOAT COMMA_CHAIN(global float*, UNIFORM_FLOAT)
    #define DEFINE_UNIFORM_FLOAT STRUCT_CHAIN(float, UNIFORM_FLOAT)
    #define SET_UNIFORM_FLOAT SET_UNIFORM_CHAIN(UNIFORM_FLOAT)
#else
    #define KERNEL_PARAM_UNIFORM_FLOAT
    #define DEFINE_UNIFORM_FLOAT
    #define SET_UNIFORM_FLOAT
#endif

#ifdef UNIFORM_INT
    #define KERNEL_PARAM_UNIFORM_INT COMMA_CHAIN(global int*, UNIFORM_INT)
    #define DEFINE_UNIFORM_INT STRUCT_CHAIN(int, UNIFORM_INT)
    #define SET_UNIFORM_INT SET_UNIFORM_CHAIN(UNIFORM_INT)
#else
    #define KERNEL_PARAM_UNIFORM_INT
    #define DEFINE_UNIFORM_INT
    #define SET_UNIFORM_INT
#endif

#ifdef UNIFORM_INT2
    #define KERNEL_PARAM_UNIFORM_INT2 COMMA_CHAIN(global int2*, UNIFORM_INT2)
    #define DEFINE_UNIFORM_INT2 STRUCT_CHAIN(int2, UNIFORM_INT2)
    #define SET_UNIFORM_INT2 SET_UNIFORM_CHAIN(UNIFORM_INT2)
#else
    #define KERNEL_PARAM_UNIFORM_INT2
    #define DEFINE_UNIFORM_INT2
    #define SET_UNIFORM_INT2
#endif

#ifdef UNIFORM_VEC4
    #define KERNEL_PARAM_UNIFORM_VEC4 COMMA_CHAIN(global float4*, UNIFORM_VEC4)
    #define DEFINE_UNIFORM_VEC4 STRUCT_CHAIN(float4, UNIFORM_VEC4)
    #define SET_UNIFORM_VEC4 SET_UNIFORM_CHAIN(UNIFORM_VEC4)
#else
    #define KERNEL_PARAM_UNIFORM_VEC4
    #define DEFINE_UNIFORM_VEC4
    #define SET_UNIFORM_VEC4
#endif

#ifdef UNIFORM_MAT4
    #define KERNEL_PARAM_UNIFORM_MAT4 COMMA_CHAIN(global float16*, UNIFORM_MAT4)
    #define DEFINE_UNIFORM_MAT4 STRUCT_CHAIN(float16, UNIFORM_MAT4)
    #define SET_UNIFORM_MAT4 SET_UNIFORM_CHAIN(UNIFORM_MAT4)
#else
    #define KERNEL_PARAM_UNIFORM_MAT4
    #define DEFINE_UNIFORM_MAT4
    #define SET_UNIFORM_MAT4
#endif

#ifdef UNIFORM_SAMPLER2D
    #define KERNEL_PARAM_UNIFORM_SAMPLER2D COMMA_CHAIN(global uint*, UNIFORM_SAMPLER2D)
    #define DEFINE_UNIFORM_SAMPLER2D STRUCT_CHAIN(uint, UNIFORM_SAMPLER2D)
    #define SET_UNIFORM_SAMPLER2D SET_UNIFORM_CHAIN(UNIFORM_SAMPLER2D)
#else
    #define KERNEL_PARAM_UNIFORM_SAMPLER2D
    #define DEFINE_UNIFORM_SAMPLER2D
    #define SET_UNIFORM_SAMPLER2D
#endif

/**
 * Uniform are defined in the vertex and fragment shaders.
 *
 * To allow the OpenCL frontend to optimize the backend calls,
 * the uniform definitions are done separately for vertex and fragment shaders.
 *
 * There is not the concept of texture unit in OpenCL world,
 * so each shader can have its own set of samplers, and the uniform
 * that hands the texture unit access is done at the OpenGL frontend.
 */

#ifdef VS_UNIFORM_FLOAT
    #define VS_KERNEL_PARAM_UNIFORM_FLOAT COMMA_CHAIN(global float*, VS_UNIFORM_FLOAT)
#else
    #define VS_KERNEL_PARAM_UNIFORM_FLOAT
#endif

#ifdef VS_UNIFORM_INT
    #define VS_KERNEL_PARAM_UNIFORM_INT COMMA_CHAIN(global int*, VS_UNIFORM_INT)
#else
    #define VS_KERNEL_PARAM_UNIFORM_INT
#endif

#ifdef VS_UNIFORM_INT2
    #define VS_KERNEL_PARAM_UNIFORM_INT2 COMMA_CHAIN(global int2*, VS_UNIFORM_INT2)
#else
    #define VS_KERNEL_PARAM_UNIFORM_INT2
#endif

#ifdef VS_UNIFORM_VEC4
    #define VS_KERNEL_PARAM_UNIFORM_VEC4 COMMA_CHAIN(global float4*, VS_UNIFORM_VEC4)
#else
    #define VS_KERNEL_PARAM_UNIFORM_VEC4
#endif

#ifdef VS_UNIFORM_MAT4
    #define VS_KERNEL_PARAM_UNIFORM_MAT4 COMMA_CHAIN(global float16*, VS_UNIFORM_MAT4)
#else
    #define VS_KERNEL_PARAM_UNIFORM_MAT4
#endif

#ifdef VS_UNIFORM_SAMPLER2D
    #define VS_KERNEL_PARAM_UNIFORM_IMAGE2D COMMA_CHAIN(image2D_t, PREPEND_CHAIN(gl_image2D_, VS_UNIFORM_SAMPLER2D))
    #define VS_KERNEL_PARAM_UNIFORM_SAMPLER2D COMMA_CHAIN(sampler2D_t, VS_UNIFORM_SAMPLER2D)
#else
    #define VS_KERNEL_PARAM_UNIFORM_IMAGE2D
    #define VS_KERNEL_PARAM_UNIFORM_SAMPLER2D
#endif

//---------------------

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

//---------------------

#define DEFINE_UNIFORMS \
    DEFINE_UNIFORM_MAT4 \
    DEFINE_UNIFORM_VEC4 \
    DEFINE_UNIFORM_INT2 \
    DEFINE_UNIFORM_SAMPLER2D \
    DEFINE_UNIFORM_INT \
    DEFINE_UNIFORM_FLOAT

#define KERNEL_PARAM_UNIFORMS \
    KERNEL_PARAM_UNIFORM_MAT4 \
    KERNEL_PARAM_UNIFORM_VEC4 \
    KERNEL_PARAM_UNIFORM_INT2 \
    KERNEL_PARAM_UNIFORM_SAMPLER2D \
    KERNEL_PARAM_UNIFORM_INT \
    KERNEL_PARAM_UNIFORM_FLOAT

#define VS_KERNEL_PARAM_UNIFORMS \
    VS_KERNEL_PARAM_UNIFORM_MAT4 \
    VS_KERNEL_PARAM_UNIFORM_VEC4 \
    VS_KERNEL_PARAM_UNIFORM_INT2 \
    VS_KERNEL_PARAM_UNIFORM_SAMPLER2D \
    VS_KERNEL_PARAM_UNIFORM_INT \
    VS_KERNEL_PARAM_UNIFORM_FLOAT 

#define FS_KERNEL_PARAM_UNIFORMS \
    FS_KERNEL_PARAM_UNIFORM_MAT4 \
    FS_KERNEL_PARAM_UNIFORM_VEC4 \
    FS_KERNEL_PARAM_UNIFORM_INT2 \
    FS_KERNEL_PARAM_UNIFORM_SAMPLER2D \
    FS_KERNEL_PARAM_UNIFORM_INT \
    FS_KERNEL_PARAM_UNIFORM_FLOAT 

#define SET_UNIFORMS \
    { \
        SET_UNIFORM_MAT4 \
        SET_UNIFORM_VEC4 \
        SET_UNIFORM_INT2 \
        SET_UNIFORM_SAMPLER2D \
        SET_UNIFORM_INT \
        SET_UNIFORM_FLOAT \
    }

//--------------------------
// Vertex shader definitions
//--------------------------

#define VS_DEFINES \
    float4 gl_Position; \
    DEFINE_VARYINGS \
    DEFINE_ATTRIBUTES \
    DEFINE_UNIFORMS

#define VS_SETS \
    SET_ATTRIBUTES \
    SET_UNIFORMS

#define VS_KERNEL_PARAMS \
    KERNEL_PARAM_ATTRIBUTES \
    VS_KERNEL_PARAM_UNIFORM_IMAGE2D

typedef struct {
    float4 gl_Position;
    OUTPUT_VARYINGS
} vertex_shader_output_t;

//----------------------------
// Fragment shader definitions
//----------------------------

#define FS_DEFINES \
    float4 gl_FragColor; \
    DEFINE_VARYINGS \
    DEFINE_UNIFORMS

#define FS_SETS \
    SET_VARYINGS \
    SET_UNIFORMS



#define TA_REPEAT_1(_S) TEXTURE_UNIT_ARG(_S##_0)
#define TA_REPEAT_2(_S) TA_REPEAT_1(_S), TEXTURE_UNIT_ARG(_S##_1) 
#define TA_REPEAT_3(_S) TA_REPEAT_2(_S), TEXTURE_UNIT_ARG(_S##_2) 
#define TA_REPEAT_4(_S) TA_REPEAT_3(_S), TEXTURE_UNIT_ARG(_S##_3) 
#define TA_REPEAT_5(_S) TA_REPEAT_4(_S), TEXTURE_UNIT_ARG(_S##_4) 
#define TA_REPEAT_6(_S) TA_REPEAT_5(_S), TEXTURE_UNIT_ARG(_S##_5) 
#define TA_REPEAT_7(_S) TA_REPEAT_6(_S), TEXTURE_UNIT_ARG(_S##_6) 
#define TA_REPEAT_8(_S) TA_REPEAT_7(_S), TEXTURE_UNIT_ARG(_S##_7) 

#define TA_REPEAT(_N, _S) TA_REPEAT_##_N(_S)

#define TP_REPEAT_1(_S) TEXTURE_UNIT_PARAM(_S##_0)
#define TP_REPEAT_2(_S) TP_REPEAT_1(_S), TEXTURE_UNIT_PARAM(_S##_1) 
#define TP_REPEAT_3(_S) TP_REPEAT_2(_S), TEXTURE_UNIT_PARAM(_S##_2) 
#define TP_REPEAT_4(_S) TP_REPEAT_3(_S), TEXTURE_UNIT_PARAM(_S##_3) 
#define TP_REPEAT_5(_S) TP_REPEAT_4(_S), TEXTURE_UNIT_PARAM(_S##_4) 
#define TP_REPEAT_6(_S) TP_REPEAT_5(_S), TEXTURE_UNIT_PARAM(_S##_5) 
#define TP_REPEAT_7(_S) TP_REPEAT_6(_S), TEXTURE_UNIT_PARAM(_S##_6) 
#define TP_REPEAT_8(_S) TP_REPEAT_7(_S), TEXTURE_UNIT_PARAM(_S##_7) 

#define TP_REPEAT(_N, _S) TP_REPEAT_##_N(_S)

// TODO: compiler aware of DEVICE_TEXTURE_UNITS

#if DEVICE_TEXTURE_UNITS != 8
    #error DEVICE_TEXTURE_UNITS must be 8
#endif

#ifndef FS_KERNEL_ARGS
#define FS_KERNEL_ARGS \
    TA_REPEAT(8, gl_texture_unit), \
    gl_texture_datas,
#endif // FS_KERNEL_ARGS

#ifndef FS_KERNEL_PARAMS
#define FS_KERNEL_PARAMS \
    TP_REPEAT(8, gl_texture_unit), \
    global const texture_data_t* gl_texture_datas,
#endif // FS_KERNEL_PARAMS

//------------------------------------
// Data reflection for OpenGL frontend
//------------------------------------

kernel void gl_varying_data(
    PARAM_VARYINGS
    private int test
) {
    return;
}

kernel void gl_attribute_data(
    KERNEL_PARAM_ATTRIBUTES
    private int test
) {
    return;
}

kernel void gl_uniform_data(
    KERNEL_PARAM_UNIFORMS
    private int test
) {
    return;
}

kernel void gl_vs_uniform_data(
    VS_KERNEL_PARAM_UNIFORMS
    private int test
) {
    return;
}

kernel void gl_fs_uniform_data(
    FS_KERNEL_PARAM_UNIFORMS
    private int test
) {
    return;
}
//-----------------------------------------------
// Include vertex and fragment shader definitions
//-----------------------------------------------

#ifdef __COMPILER_RELATIVE_PATH__
    #include <backend/shaders/vertex.cl>
    #include <backend/shaders/fragment.cl>
#else
    #include "glsc2/src/backend/shaders/vertex.cl"
    #include "glsc2/src/backend/shaders/fragment.cl"
#endif

#endif


