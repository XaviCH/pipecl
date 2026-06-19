#ifndef BACKEND_SHADERS_VERTEX_CL
#define BACKEND_SHADERS_VERTEX_CL

#include <backend/shaders/wrapper.cl>
#include <backend/types.cl>
#include <backend/utils/common.cl>
#include <constants.device.h>

// vertex attribute definitions

#define READ_COMPONENT_float(base, i)  as_float(       buffer_load_u32(base, i))
#define READ_COMPONENT_char(base, i)   ((float)(char)  buffer_load_u8 (base, i))
#define READ_COMPONENT_uchar(base, i)  ((float)        buffer_load_u8 (base, i))
#define READ_COMPONENT_short(base, i)  ((float)(short) buffer_load_u16(base, i))
#define READ_COMPONENT_ushort(base, i) ((float)        buffer_load_u16(base, i))

#define GENERIC_GET_FLOAT4_FROM_X(_X) \
    static inline float4 gl_get_float4_from_##_X(ro_buffer_t base, uint size) \
    { \
        float4 result = {0,0,0,1}; \
        switch (size) { \
            case VERTEX_ATTRIBUTE_SIZE_4: \
                result.w = READ_COMPONENT_##_X(base, 3); \
            case VERTEX_ATTRIBUTE_SIZE_3: \
                result.z = READ_COMPONENT_##_X(base, 2); \
            case VERTEX_ATTRIBUTE_SIZE_2: \
                result.y = READ_COMPONENT_##_X(base, 1); \
            default: \
            case VERTEX_ATTRIBUTE_SIZE_1: \
                result.x = READ_COMPONENT_##_X(base, 0); \
        } \
        return result; \
    }

GENERIC_GET_FLOAT4_FROM_X(float);
GENERIC_GET_FLOAT4_FROM_X(char);
GENERIC_GET_FLOAT4_FROM_X(uchar);
GENERIC_GET_FLOAT4_FROM_X(short);
GENERIC_GET_FLOAT4_FROM_X(ushort);

#undef GENERIC_GET_FLOAT4_FROM_X
#undef READ_COMPONENT_float
#undef READ_COMPONENT_char
#undef READ_COMPONENT_uchar
#undef READ_COMPONENT_short
#undef READ_COMPONENT_ushort

// ------

static inline float4 gl_get_vertex_attribute_from_pointer(
    ro_buffer_t data,
    vertex_attribute_data_t vertex_attribute_data
) {
    size_t id = get_global_linear_id();

    size_t       byte_offset = vertex_attribute_data.offset + vertex_attribute_data.stride * id;
    ro_buffer_t  base        = buffer_offset_u8(data, byte_offset);

    uint type = gl_get_vertex_attribute_type(vertex_attribute_data);
    uint size = gl_get_vertex_attribute_size(vertex_attribute_data);

    float4 result;

    switch (type)
    {
        default:
        case VERTEX_ATTRIBUTE_TYPE_FLOAT:
            return gl_get_float4_from_float(base, size); // never normalized
        case VERTEX_ATTRIBUTE_TYPE_BYTE:
            result = gl_get_float4_from_char(base, size); break;
        case VERTEX_ATTRIBUTE_TYPE_UNSIGNED_BYTE:
            result = gl_get_float4_from_uchar(base, size); break;
        case VERTEX_ATTRIBUTE_TYPE_SHORT:
            result = gl_get_float4_from_short(base, size); break;
        case VERTEX_ATTRIBUTE_TYPE_UNSIGNED_SHORT:
            result = gl_get_float4_from_ushort(base, size); break;
    }
    
    bool normalize = gl_get_vertex_attribute_normalize(vertex_attribute_data);

    if (!normalize) return result;

    switch (type)
    {
        default:
            return result;
        case VERTEX_ATTRIBUTE_TYPE_BYTE:
            return max(result / (float4){CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX}, (float4){-1.0f,-1.0f,-1.0f,-1.0f});
        case VERTEX_ATTRIBUTE_TYPE_UNSIGNED_BYTE:
            return result / (float4){UCHAR_MAX, UCHAR_MAX, UCHAR_MAX, UCHAR_MAX};
        case VERTEX_ATTRIBUTE_TYPE_SHORT:
            return max(result / (float4){SHRT_MAX, SHRT_MAX, SHRT_MAX, SHRT_MAX}, (float4){-1.0f,-1.0f,-1.0f,-1.0f});
        case VERTEX_ATTRIBUTE_TYPE_UNSIGNED_SHORT:
            return result / (float4){USHRT_MAX, USHRT_MAX, USHRT_MAX, USHRT_MAX};
    }
    
}

static inline float4 gl_get_vertex_attribute(
    global const float4* restrict vertex_attributes, 
    global const vertex_attribute_data_t* restrict vertex_attribute_datas,
    ro_buffer_t restrict attribute_pointer,
    int attribute_location
) {
    if ((vertex_attribute_datas[attribute_location].misc & VERTEX_ATTRIBUTE_ACTIVE_POINTER) != 0)
        return gl_get_vertex_attribute_from_pointer(attribute_pointer, vertex_attribute_datas[attribute_location]);
    
    return vertex_attributes[attribute_location];
}

#define GL_SET_VERTEX_ATTRIBUTE_ARGS \
    ro_buffer_t attribute_pointer, \
    global const float4* vertex_attributes, \
    global const vertex_attribute_data_t* vertex_attribute_datas, \
    int attribute_location

inline void __attribute__((overloadable)) gl_set_vertex_attribute(private float2* dst, GL_SET_VERTEX_ATTRIBUTE_ARGS)
{
    *dst = gl_get_vertex_attribute(vertex_attributes, vertex_attribute_datas, attribute_pointer, attribute_location).xy;
}
inline void __attribute__((overloadable)) gl_set_vertex_attribute(private float3* dst, GL_SET_VERTEX_ATTRIBUTE_ARGS)
{
    *dst = gl_get_vertex_attribute(vertex_attributes, vertex_attribute_datas, attribute_pointer, attribute_location).xyz;
}
inline void __attribute__((overloadable)) gl_set_vertex_attribute(private float4* dst, GL_SET_VERTEX_ATTRIBUTE_ARGS)
{
    *dst = gl_get_vertex_attribute(vertex_attributes, vertex_attribute_datas, attribute_pointer, attribute_location);
}

#undef GL_SET_VERTEX_ATTRIBUTE_ARGS

//---------------------------
// Attribute setters (VS-only)
//---------------------------

#define SET_ATTRIBUTE(attr) \
{ \
    gl_set_vertex_attribute(&attr, _##attr, vertex_attributes, vertex_attribute_datas, gl_attribute_location); \
    gl_attribute_location += 1; \
}

#define OP_SETATTR(ctx, a) SET_ATTRIBUTE(a);
#define SET_ATTRIBUTE_CHAIN(...) FOR_EACH(OP_SETATTR, ~, __VA_ARGS__)

//----------------------
// Attribute definitions
//----------------------

#ifdef ATTRIBUTE_VEC2
    #define KERNEL_ARG_ATTRIBUTE_VEC2 COMMA_CHAIN(ro_buffer_t, JOIN_CHAIN(_,ATTRIBUTE_VEC2))
    #define DEFINE_ATTRIBUTE_VEC2 STRUCT_CHAIN(float2, ATTRIBUTE_VEC2)
    #define SET_ATTRIBUTE_VEC2 SET_ATTRIBUTE_CHAIN(ATTRIBUTE_VEC2)
#else
    #define KERNEL_ARG_ATTRIBUTE_VEC2
    #define DEFINE_ATTRIBUTE_VEC2
    #define SET_ATTRIBUTE_VEC2
#endif

#ifdef ATTRIBUTE_VEC3
    #define KERNEL_ARG_ATTRIBUTE_VEC3 COMMA_CHAIN(ro_buffer_t, JOIN_CHAIN(_,ATTRIBUTE_VEC3))
    #define DEFINE_ATTRIBUTE_VEC3 STRUCT_CHAIN(float3, ATTRIBUTE_VEC3)
    #define SET_ATTRIBUTE_VEC3 SET_ATTRIBUTE_CHAIN(ATTRIBUTE_VEC3)
#else
    #define KERNEL_ARG_ATTRIBUTE_VEC3
    #define DEFINE_ATTRIBUTE_VEC3
    #define SET_ATTRIBUTE_VEC3
#endif

#ifdef ATTRIBUTE_VEC4
    #define KERNEL_ARG_ATTRIBUTE_VEC4 COMMA_CHAIN(ro_buffer_t, JOIN_CHAIN(_,ATTRIBUTE_VEC4))
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

//------------------------------------------------------
// vertex uniform params, only for frontend optimization
//------------------------------------------------------

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

#define VS_KERNEL_PARAM_UNIFORMS \
    VS_KERNEL_PARAM_UNIFORM_MAT4 \
    VS_KERNEL_PARAM_UNIFORM_VEC4 \
    VS_KERNEL_PARAM_UNIFORM_INT2 \
    VS_KERNEL_PARAM_UNIFORM_SAMPLER2D \
    VS_KERNEL_PARAM_UNIFORM_INT \
    VS_KERNEL_PARAM_UNIFORM_FLOAT

//--------------------------
// vertex shader kernel definitions
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

#define OP_VS_MOVE(c, _NAME) APPLY(EXP_VS_MOVE, UNPAREN c, _NAME)
#define EXP_VS_MOVE(_BUFFER, _LOCATION, _STRUCT, _NAME) _BUFFER[_LOCATION++] = _STRUCT._NAME;
#define VS_MOVE_OUTPUT_TO_ARRAY_CHAIN(_BUFFER, _LOCATION, _STRUCT, ...) FOR_EACH(OP_VS_MOVE, (_BUFFER, _LOCATION, _STRUCT), __VA_ARGS__)

#ifdef VARYING_FLOAT
    #define VS_MOVE_OUTPUT_FLOAT_TO_ARRAY(_BUFFER, _LOCATION, _STRUCT) VS_MOVE_OUTPUT_TO_ARRAY_CHAIN(_BUFFER, _LOCATION, _STRUCT, VARYING_FLOAT)
    #define VS_VARYING_FLOAT_COUNT COUNT(VARYING_FLOAT)
#else
    #define VS_MOVE_OUTPUT_FLOAT_TO_ARRAY(...)
    #define VS_VARYING_FLOAT_COUNT 0
#endif

#ifdef VARYING_VEC2
    #define VS_MOVE_OUTPUT_VEC2_TO_ARRAY(_BUFFER, _LOCATION, _STRUCT) VS_MOVE_OUTPUT_TO_ARRAY_CHAIN(_BUFFER, _LOCATION, _STRUCT, VARYING_VEC2)
    #define VS_VARYING_VEC2_COUNT COUNT(VARYING_VEC2)
#else
    #define VS_MOVE_OUTPUT_VEC2_TO_ARRAY(...)
    #define VS_VARYING_VEC2_COUNT 0
#endif

#ifdef VARYING_VEC3
    #define VS_MOVE_OUTPUT_VEC3_TO_ARRAY(_BUFFER, _LOCATION, _STRUCT) VS_MOVE_OUTPUT_TO_ARRAY_CHAIN(_BUFFER, _LOCATION, _STRUCT, VARYING_VEC3)
    #define VS_VARYING_VEC3_COUNT COUNT(VARYING_VEC3)
#else
    #define VS_MOVE_OUTPUT_VEC3_TO_ARRAY(...)
    #define VS_VARYING_VEC3_COUNT 0
#endif

#ifdef VARYING_VEC4
    #define VS_MOVE_OUTPUT_VEC4_TO_ARRAY(_BUFFER, _LOCATION, _STRUCT) VS_MOVE_OUTPUT_TO_ARRAY_CHAIN(_BUFFER, _LOCATION, _STRUCT, VARYING_VEC4)
    #define VS_VARYING_VEC4_COUNT COUNT(VARYING_VEC4)
#else
    #define VS_MOVE_OUTPUT_VEC4_TO_ARRAY(...)
    #define VS_VARYING_VEC4_COUNT 0
#endif

#define VS_MOVE_OUTPUT_TO_ARRAY(_BUFFER, _OUTPUT) \
    { \
        _BUFFER[0] = _OUTPUT.gl_Position; \
        uint varying_location = 1; \
        VS_MOVE_OUTPUT_FLOAT_TO_ARRAY(_BUFFER, varying_location, _OUTPUT) \
        VS_MOVE_OUTPUT_VEC2_TO_ARRAY(_BUFFER, varying_location, _OUTPUT) \
        VS_MOVE_OUTPUT_VEC3_TO_ARRAY(_BUFFER, varying_location, _OUTPUT) \
        VS_MOVE_OUTPUT_VEC4_TO_ARRAY(_BUFFER, varying_location, _OUTPUT) \
    }

#define NUM_VARYING (VS_VARYING_FLOAT_COUNT + VS_VARYING_VEC2_COUNT + VS_VARYING_VEC3_COUNT + VS_VARYING_VEC4_COUNT)

static inline void gl_fill_vertex_buffer(
    private vertex_shader_output_t output, 
    wo_vertex_buffer_t vertex_buffer
) {
    const uint output_size = 1 + NUM_VARYING; // +1 for gl_Position

    float4 buffer[1 + NUM_VARYING]; // +1 for gl_Position

    VS_MOVE_OUTPUT_TO_ARRAY(buffer, output);

    size_t id = get_global_id(0); // Enabled for multiple kernels synchronously writing to the same buffer.
    uint index_start = id*output_size;

    #pragma unroll
    for(uint attrib = 0; attrib < output_size; ++attrib) {
        write_vertex_buffer(vertex_buffer, index_start + attrib, buffer[attrib]);
    }
}

/**
 * TODO: 
 *     Implement constant memory.
 *     KERNEL_ARG_ATTRIBUTES could be one dimensional array,
 *     and vertex_attribute_datas could contain the offsets.
 */
#ifdef DEVICE_SUBBUFFER_ENABLED
#define PRIMITIVE_IDX_PARAM
#define SET_PRIMITIVE_IDX cl_uint primitive_id = 0; 
#else
#define PRIMITIVE_IDX_PARAM ,const cl_uint primitive_id 
#define SET_PRIMITIVE_IDX 
#endif

#define VS_MAIN(...) \
    kernel \
    __attribute__((reqd_work_group_size(DEVICE_VERTEX_THREADS, 1, 1))) \
    void gl_vertex_shader( \
        global const float4* restrict vertex_attributes, \
        global const vertex_attribute_data_t* restrict vertex_attribute_datas, \
        uniform_addr_space uniform_buffer_t* restrict gl_uniforms, \
        VS_KERNEL_PARAMS \
        wo_vertex_buffer_t vertex_buffer, \
        const uint c_num_vertices \
        PRIMITIVE_IDX_PARAM \
    ) { \
        /* Define accessible objects from vertex shader */ \
        VS_DEFINES \
        if (get_global_linear_id() >= c_num_vertices) return; \
        /* Set values from buffers */ \
        SET_PRIMITIVE_IDX \
        VS_SETS \
        /* Run vertex shader */ \
        __VA_ARGS__ \
        /* Fill vertex buffer */ \
        vertex_shader_output_t output; \
        output.gl_Position = gl_Position; \
        SET_STRUCT_VARYINGS \
        gl_fill_vertex_buffer(output, vertex_buffer); \
    }

//-----------------------------
// data reflection for frontend
//-----------------------------

kernel void gl_attribute_data(
    KERNEL_PARAM_ATTRIBUTES
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

#endif // BACKEND_SHADERS_VERTEX_CL