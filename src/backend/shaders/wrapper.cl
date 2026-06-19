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

#include <constants.device.h>
#include <backend/shaders/glsl/macros.h>

//--------------------
// Varying definitions
//--------------------

static inline __attribute__((overloadable)) float4 convert_varying(float  a) { return (float4){a, 0, 0, 1}; }
static inline __attribute__((overloadable)) float4 convert_varying(float2 a) { return (float4){a, 0, 1};    }
static inline __attribute__((overloadable)) float4 convert_varying(float3 a) { return (float4){a, 1};       }
static inline __attribute__((overloadable)) float4 convert_varying(float4 a) { return (float4){a};          }

static inline __attribute__((overloadable)) void assign_varying(float*  d, float4 v) { *d = v.x;   }
static inline __attribute__((overloadable)) void assign_varying(float2* d, float4 v) { *d = v.xy;  }
static inline __attribute__((overloadable)) void assign_varying(float3* d, float4 v) { *d = v.xyz; }
static inline __attribute__((overloadable)) void assign_varying(float4* d, float4 v) { *d = v;     }

// To name.a = convert_varying(a);
#define OP_SET_VARYING_TO_STRUCT(name, a) name.a = convert_varying(a);
#define SET_VARYING_TO_STRUCT_CHAIN(name, ...) FOR_EACH(OP_SET_VARYING_TO_STRUCT, name, __VA_ARGS__)


#ifdef VARYING_FLOAT
    #define PARAM_VARYING_FLOAT COMMA_CHAIN(float, VARYING_FLOAT)
    #define DEFINE_VARYING_FLOAT STRUCT_CHAIN(float, VARYING_FLOAT)
    #define OUTPUT_VARYING_FLOAT STRUCT_CHAIN(float4, VARYING_FLOAT)
    #define SET_STRUCT_VARYING_FLOAT SET_VARYING_TO_STRUCT_CHAIN(output, VARYING_FLOAT)
    #define SET_VARYING_FLOAT SET_VARYING_CHAIN(VARYING_FLOAT)
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
    #define SET_STRUCT_VARYING_VEC2 SET_VARYING_TO_STRUCT_CHAIN(output, VARYING_VEC2)
    // fs interpolation also fills the <name>_grad variables for auto-LOD.
    #define SET_VARYING_VEC2 SET_VARYING_VEC2_GRAD(VARYING_VEC2)
    #define FS_DEFINE_VARYING_VEC2_GRAD DEFINE_VARYING_VEC2_GRAD(VARYING_VEC2)
#else
    #define PARAM_VARYING_VEC2
    #define DEFINE_VARYING_VEC2
    #define OUTPUT_VARYING_VEC2
    #define SET_STRUCT_VARYING_VEC2
    #define SET_VARYING_VEC2
    #define FS_DEFINE_VARYING_VEC2_GRAD
#endif

#ifdef VARYING_VEC3
    #define PARAM_VARYING_VEC3 COMMA_CHAIN(float3, VARYING_VEC3)
    #define DEFINE_VARYING_VEC3 STRUCT_CHAIN(float3, VARYING_VEC3)
    #define OUTPUT_VARYING_VEC3 STRUCT_CHAIN(float4, VARYING_VEC3)
    #define SET_STRUCT_VARYING_VEC3 SET_VARYING_TO_STRUCT_CHAIN(output, VARYING_VEC3)
    #define SET_VARYING_VEC3 SET_VARYING_CHAIN(VARYING_VEC3)
#else
    #define PARAM_VARYING_VEC3
    #define DEFINE_VARYING_VEC3
    #define OUTPUT_VARYING_VEC3
    #define SET_STRUCT_VARYING_VEC3
    #define SET_VARYING_VEC3
#endif

#ifdef VARYING_VEC4
    #define PARAM_VARYING_VEC4 COMMA_CHAIN(float4, VARYING_VEC4)
    #define DEFINE_VARYING_VEC4 STRUCT_CHAIN(float4, VARYING_VEC4)
    #define OUTPUT_VARYING_VEC4 STRUCT_CHAIN(float4, VARYING_VEC4)
    #define SET_STRUCT_VARYING_VEC4 SET_VARYING_TO_STRUCT_CHAIN(output, VARYING_VEC4)
    #define SET_VARYING_VEC4 SET_VARYING_CHAIN(VARYING_VEC4)
#else
    #define PARAM_VARYING_VEC4
    #define DEFINE_VARYING_VEC4
    #define OUTPUT_VARYING_VEC4
    #define SET_STRUCT_VARYING_VEC4
    #define SET_VARYING_VEC4
#endif

#define VARYING_LIST(X) X(FLOAT) X(VEC2) X(VEC3) X(VEC4)

#define V_DEFINE(s)     DEFINE_VARYING_##s
#define V_OUTPUT(s)     OUTPUT_VARYING_##s
#define V_PARAM(s)      PARAM_VARYING_##s
#define V_SET_STRUCT(s) SET_STRUCT_VARYING_##s
#define V_SET(s)        SET_VARYING_##s

#define DEFINE_VARYINGS     VARYING_LIST(V_DEFINE)
#define OUTPUT_VARYINGS     VARYING_LIST(V_OUTPUT)
#define PARAM_VARYINGS      VARYING_LIST(V_PARAM)
#define SET_STRUCT_VARYINGS VARYING_LIST(V_SET_STRUCT)
#define SET_VARYINGS        { uint varying = 0; VARYING_LIST(V_SET) }

//--------------------
// Uniform definitions
//--------------------

/**
 * Uniform are defined in the vertex and fragment shaders.
 *
 * To allow the OpenCL frontend to optimize the backend calls,
 * the uniform definitions are done separately for vertex and fragment shaders.
 *
 */

// uniform types definitions
#ifdef UNIFORM_MAT4
    #define UNIFORM_ROW_MAT4(X) X(float16, UNIFORM_MAT4)
#else
    #define UNIFORM_ROW_MAT4(X)
#endif
#ifdef UNIFORM_MAT2
    #define UNIFORM_ROW_MAT2(X) X(float8, UNIFORM_MAT2)
#else
    #define UNIFORM_ROW_MAT2(X)
#endif
#ifdef UNIFORM_VEC4
    #define UNIFORM_ROW_VEC4(X) X(float4, UNIFORM_VEC4)
#else
    #define UNIFORM_ROW_VEC4(X)
#endif
#ifdef UNIFORM_INT4
    #define UNIFORM_ROW_INT4(X) X(int4, UNIFORM_INT4)
#else
    #define UNIFORM_ROW_INT4(X)
#endif
#ifdef UNIFORM_VEC2
    #define UNIFORM_ROW_VEC2(X) X(float2, UNIFORM_VEC2)
#else
    #define UNIFORM_ROW_VEC2(X)
#endif
#ifdef UNIFORM_INT2
    #define UNIFORM_ROW_INT2(X) X(int2, UNIFORM_INT2)
#else
    #define UNIFORM_ROW_INT2(X)
#endif
#ifdef UNIFORM_SAMPLER2D
    #define UNIFORM_ROW_SAMPLER2D(X) X(uint, UNIFORM_SAMPLER2D)
#else
    #define UNIFORM_ROW_SAMPLER2D(X)
#endif
#ifdef UNIFORM_INT
    #define UNIFORM_ROW_INT(X) X(int, UNIFORM_INT)
#else
    #define UNIFORM_ROW_INT(X)
#endif
#ifdef UNIFORM_FLOAT
    #define UNIFORM_ROW_FLOAT(X) X(float, UNIFORM_FLOAT)
#else
    #define UNIFORM_ROW_FLOAT(X)
#endif

// ordered by type size
#define UNIFORM_TABLE(X) \
    UNIFORM_ROW_MAT4(X) \
    UNIFORM_ROW_MAT2(X) \
    UNIFORM_ROW_VEC4(X) \
    UNIFORM_ROW_INT4(X) \
    UNIFORM_ROW_VEC2(X) \
    UNIFORM_ROW_INT2(X) \
    UNIFORM_ROW_SAMPLER2D(X) \
    UNIFORM_ROW_INT(X) \
    UNIFORM_ROW_FLOAT(X)


// Per-element uniform struct members and setters; built on FOR_EACH (glsl/macros.h).

#define GEN_DEFINE_UNIFORM(ctype, name) STRUCT_CHAIN(ctype, name)
#define DEFINE_UNIFORMS       UNIFORM_TABLE(GEN_DEFINE_UNIFORM)

#define OP_SET_UNIFORM(ctx, a) a = gl_uniforms->a;
#define SET_UNIFORM_CHAIN(...) FOR_EACH(OP_SET_UNIFORM, ~, __VA_ARGS__)
#define GEN_SET_UNIFORM(ctype, name)    SET_UNIFORM_CHAIN(name)
#define SET_UNIFORMS        { UNIFORM_TABLE(GEN_SET_UNIFORM) }

// DEFINE_UNIFORMS is now defined, so types.device.h's uniform_buffer_t union can
// expand its typed members. Pull in the built-in functions (and, transitively, the
// base type headers) here rather than at the top.
#include <backend/shaders/glsl/built_in.cl>

//----------------------------------
// vertex shader output struct
//----------------------------------

typedef struct {
    float4 gl_Position;
    OUTPUT_VARYINGS
} vertex_shader_output_t;

//------------------------------------
// data reflection for OpenGL frontend
//------------------------------------

kernel void gl_varying_data(
    PARAM_VARYINGS
    private int test
) {
    return;
}

#define GEN_PARAM_UNIFORM(ctype, name)  COMMA_CHAIN(global ctype*, name)

kernel void gl_uniform_data(
    UNIFORM_TABLE(GEN_PARAM_UNIFORM)
    private int test
) {
    return;
}

#undef GEN_PARAM_UNIFORM

//-----------------------------------------------
// Include vertex and fragment shader definitions
//-----------------------------------------------

#include <backend/shaders/vertex.cl>
#include <backend/shaders/fragment.cl>

#endif
