#ifndef BACKEND_SHADERS_VERTEX_CL
#define BACKEND_SHADERS_VERTEX_CL

#ifdef __COMPILER_RELATIVE_PATH__
    #include <backend/shaders/wrapper.cl>
    #include <backend/types.cl>
    #include <backend/utils/common.cl>
    #include <constants.device.h>
#else
    #include "glsc2/src/backend/shaders/wrapper.cl"
    #include "glsc2/src/backend/types.cl"
    #include "glsc2/src/backend/utils/common.cl"
    #include "glsc2/src/constants.device.h"
#endif

#define GENERIC_GET_FLOAT4_FROM_X(_X) \
    static inline float4 gl_get_float4_from_##_X(global _X* ptr, uint size) \
    { \
        float4 result = {0,0,0,1}; \
        switch (size) { \
            case VERTEX_ATTRIBUTE_SIZE_4: \
                result.w = (float) ptr[3]; \
            case VERTEX_ATTRIBUTE_SIZE_3: \
                result.z = (float) ptr[2]; \
            case VERTEX_ATTRIBUTE_SIZE_2: \
                result.y = (float) ptr[1]; \
            default: \
            case VERTEX_ATTRIBUTE_SIZE_1: \
                result.x = (float) ptr[0]; \
        } \
        return result; \
    }

GENERIC_GET_FLOAT4_FROM_X(float);
GENERIC_GET_FLOAT4_FROM_X(char);
GENERIC_GET_FLOAT4_FROM_X(uchar);
GENERIC_GET_FLOAT4_FROM_X(short);
GENERIC_GET_FLOAT4_FROM_X(ushort);

#undef GENERIC_GET_FLOAT4_FROM_X

// ------

static inline float4 gl_get_vertex_attribute_from_pointer(
    global const void* data, 
    vertex_attribute_data_t vertex_attribute_data
) {
    size_t id = get_global_linear_id();

    data = (global const void*) ( (global const uchar*) data + vertex_attribute_data.offset + (vertex_attribute_data.stride) * id);

    uint type = gl_get_vertex_attribute_type(vertex_attribute_data);
    uint size = gl_get_vertex_attribute_size(vertex_attribute_data);

    float4 result;

    switch (type)
    {
        default:
        case VERTEX_ATTRIBUTE_TYPE_FLOAT:
            return gl_get_float4_from_float((global float*) data, size);
        case VERTEX_ATTRIBUTE_TYPE_BYTE:
            result = gl_get_float4_from_char((global char*) data, size);
            break;
        case VERTEX_ATTRIBUTE_TYPE_UNSIGNED_BYTE:
            result = gl_get_float4_from_uchar((global uchar*) data, size);
            break;
        case VERTEX_ATTRIBUTE_TYPE_SHORT:
            result = gl_get_float4_from_short((global short*) data, size);
            break;
        case VERTEX_ATTRIBUTE_TYPE_UNSIGNED_SHORT:
            result = gl_get_float4_from_ushort((global ushort*) data, size);
            break;
    }
    
    bool normalize = gl_get_vertex_attribute_normalize(vertex_attribute_data);

    if (!normalize) return result;

    switch (type)
    {
        default:
            return result;
        case VERTEX_ATTRIBUTE_TYPE_BYTE:
            return result / (float4){CHAR_MAX+1, CHAR_MAX+1, CHAR_MAX+1, CHAR_MAX+1};
        case VERTEX_ATTRIBUTE_TYPE_UNSIGNED_BYTE:
            return result / (float4){UCHAR_MAX, UCHAR_MAX, UCHAR_MAX, UCHAR_MAX};
        case VERTEX_ATTRIBUTE_TYPE_SHORT:
            return result / (float4){SHRT_MAX+1, SHRT_MAX+1, SHRT_MAX+1, SHRT_MAX+1}; // TODO: check this
        case VERTEX_ATTRIBUTE_TYPE_UNSIGNED_SHORT:
            return result / (float4){USHRT_MAX, USHRT_MAX, USHRT_MAX, USHRT_MAX};
    }
    
}

inline float4 gl_get_vertex_attribute(
    global const float4* vertex_attributes, 
    global const vertex_attribute_data_t* vertex_attribute_datas,
    global const void* attribute_pointer,
    int attribute_location
) {
    if ((vertex_attribute_datas[attribute_location].misc & VERTEX_ATTRIBUTE_ACTIVE_POINTER) != 0)
        return gl_get_vertex_attribute_from_pointer(attribute_pointer, vertex_attribute_datas[attribute_location]);
    
    return vertex_attributes[attribute_location];
}

#define GL_SET_VERTEX_ATTRIBUTE_ARGS \
    global const void* attribute_pointer, \
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



#define VS_MOVE_OUTPUT_TO_ARRAY_CHAIN(_BUFFER, _LOCATION, _STRUCT, ...) GLUE(VS_MOVE_OUTPUT_TO_ARRAY_CHAIN_, COUNT(__VA_ARGS__))(_BUFFER, _LOCATION, _STRUCT, __VA_ARGS__)

#define VS_MOVE_OUTPUT_TO_ARRAY_CHAIN_1(_BUFFER, _LOCATION, _STRUCT, _NAME)      _BUFFER[_LOCATION++] = _STRUCT._NAME;
#define VS_MOVE_OUTPUT_TO_ARRAY_CHAIN_2(_BUFFER, _LOCATION, _STRUCT, _NAME, ...) _BUFFER[_LOCATION++] = _STRUCT._NAME; VS_MOVE_OUTPUT_TO_ARRAY_CHAIN_1(_BUFFER, _LOCATION, _STRUCT, __VA_ARGS__)
#define VS_MOVE_OUTPUT_TO_ARRAY_CHAIN_3(_BUFFER, _LOCATION, _STRUCT, _NAME, ...) _BUFFER[_LOCATION++] = _STRUCT._NAME; VS_MOVE_OUTPUT_TO_ARRAY_CHAIN_2(_BUFFER, _LOCATION, _STRUCT, __VA_ARGS__)
#define VS_MOVE_OUTPUT_TO_ARRAY_CHAIN_4(_BUFFER, _LOCATION, _STRUCT, _NAME, ...) _BUFFER[_LOCATION++] = _STRUCT._NAME; VS_MOVE_OUTPUT_TO_ARRAY_CHAIN_3(_BUFFER, _LOCATION, _STRUCT, __VA_ARGS__)
#define VS_MOVE_OUTPUT_TO_ARRAY_CHAIN_5(_BUFFER, _LOCATION, _STRUCT, _NAME, ...) _BUFFER[_LOCATION++] = _STRUCT._NAME; VS_MOVE_OUTPUT_TO_ARRAY_CHAIN_4(_BUFFER, _LOCATION, _STRUCT, __VA_ARGS__)
#define VS_MOVE_OUTPUT_TO_ARRAY_CHAIN_6(_BUFFER, _LOCATION, _STRUCT, _NAME, ...) _BUFFER[_LOCATION++] = _STRUCT._NAME; VS_MOVE_OUTPUT_TO_ARRAY_CHAIN_5(_BUFFER, _LOCATION, _STRUCT, __VA_ARGS__)
#define VS_MOVE_OUTPUT_TO_ARRAY_CHAIN_7(_BUFFER, _LOCATION, _STRUCT, _NAME, ...) _BUFFER[_LOCATION++] = _STRUCT._NAME; VS_MOVE_OUTPUT_TO_ARRAY_CHAIN_6(_BUFFER, _LOCATION, _STRUCT, __VA_ARGS__)
#define VS_MOVE_OUTPUT_TO_ARRAY_CHAIN_8(_BUFFER, _LOCATION, _STRUCT, _NAME, ...) _BUFFER[_LOCATION++] = _STRUCT._NAME; VS_MOVE_OUTPUT_TO_ARRAY_CHAIN_7(_BUFFER, _LOCATION, _STRUCT, __VA_ARGS__)
#define VS_MOVE_OUTPUT_TO_ARRAY_CHAIN_9(_BUFFER, _LOCATION, _STRUCT, _NAME, ...) _BUFFER[_LOCATION++] = _STRUCT._NAME; VS_MOVE_OUTPUT_TO_ARRAY_CHAIN_8(_BUFFER, _LOCATION, _STRUCT, __VA_ARGS__)


/* todo
#define REP0(X)
#define REP1(X) X
#define REP2(X) REP1(X) X
#define REP3(X) REP2(X) X
#define REP4(X) REP3(X) X
#define REP5(X) REP4(X) X
#define REP6(X) REP5(X) X
#define REP7(X) REP6(X) X
#define REP8(X) REP7(X) X
#define REP9(X) REP8(X) X
#define REP10(X) REP9(X) X

#define REP_MACRO(HUNDREDS,TENS,ONES,_FUNC,...) \
  REP_MACRO_##HUNDREDS(REP_MACRO_10(REP_MACRO_10(_FUNC, __VA_ARGS__), __VA_ARGS__), __VA_ARGS__) \
  REP_MACRO_##TENS(REP_FUNC_10(X)) \
  REP_MACRO_##ONES(X)
*/

// REMOVE PREPROCESSOR

#define REMOVE_0(...) __VA_ARGS__
#define REMOVE_1(X,...) __VA_ARGS__
#define REMOVE_2(X,...) REMOVE_1(__VA_ARGS__)
#define REMOVE_3(X,...) REMOVE_2(__VA_ARGS__)
#define REMOVE_4(X,...) REMOVE_3(__VA_ARGS__)
#define REMOVE_5(X,...) REMOVE_4(__VA_ARGS__)
#define REMOVE_6(X,...) REMOVE_5(__VA_ARGS__)
#define REMOVE_7(X,...) REMOVE_6(__VA_ARGS__)
#define REMOVE_8(X,...) REMOVE_7(__VA_ARGS__)
#define REMOVE_9(X,...) REMOVE_8(__VA_ARGS__)

#define REMOVE_00(...) __VA_ARGS__
#define REMOVE_10(...) REMOVE_1(REMOVE_9(__VA_ARGS__))
#define REMOVE_20(...) REMOVE_10(REMOVE_10(__VA_ARGS__))
#define REMOVE_30(...) REMOVE_20(REMOVE_10(__VA_ARGS__))
#define REMOVE_40(...) REMOVE_30(REMOVE_10(__VA_ARGS__))
#define REMOVE_50(...) REMOVE_40(REMOVE_10(__VA_ARGS__))
#define REMOVE_60(...) REMOVE_50(REMOVE_10(__VA_ARGS__))
#define REMOVE_70(...) REMOVE_60(REMOVE_10(__VA_ARGS__))
#define REMOVE_80(...) REMOVE_70(REMOVE_10(__VA_ARGS__))
#define REMOVE_90(...) REMOVE_80(REMOVE_10(__VA_ARGS__))

#define REMOVE_000(...) __VA_ARGS__
#define REMOVE_100(...) REMOVE_10(REMOVE_90(__VA_ARGS__))
#define REMOVE_200(...) REMOVE_100(REMOVE_100(__VA_ARGS__))
#define REMOVE_300(...) REMOVE_200(REMOVE_100(__VA_ARGS__))
#define REMOVE_400(...) REMOVE_300(REMOVE_100(__VA_ARGS__))
#define REMOVE_500(...) REMOVE_400(REMOVE_100(__VA_ARGS__))
#define REMOVE_600(...) REMOVE_500(REMOVE_100(__VA_ARGS__))
#define REMOVE_700(...) REMOVE_600(REMOVE_100(__VA_ARGS__))
#define REMOVE_800(...) REMOVE_700(REMOVE_100(__VA_ARGS__))
#define REMOVE_900(...) REMOVE_800(REMOVE_100(__VA_ARGS__))

#define REMOVE(HUNDREDS,TENS,ONES, ...) \
    REMOVE_##HUNDREDS##00(REMOVE_##TENS##0(REMOVE_##ONES(__VA_ARGS__)))

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
        global const float4* vertex_attributes, \
        global const vertex_attribute_data_t* vertex_attribute_datas, \
        global void* gl_uniforms, \
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
        uint gl_uniform_offset = 0 * DEVICE_UNIFORM_CAPACITY; \
        VS_SETS \
        /* Run vertex shader */ \
        __VA_ARGS__ \
        /* Fill vertex buffer */ \
        vertex_shader_output_t output; \
        output.gl_Position = gl_Position; \
        SET_STRUCT_VARYINGS \
        gl_fill_vertex_buffer(output, vertex_buffer); \
    }

#endif // BACKEND_SHADERS_VERTEX_CL