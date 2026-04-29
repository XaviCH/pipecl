#ifndef BACKEND_SHADERS_FRAGMENT_CL
#define BACKEND_SHADERS_FRAGMENT_CL

#ifdef __COMPILER_RELATIVE_PATH__
    #include <backend/shaders/wrapper.cl>
    #include <backend/types.cl>
    #include <constants.device.h>
#else
    #include "glsc2/src/backend/shaders/wrapper.cl"
    #include "glsc2/src/backend/types.cl"
    #include "glsc2/src/constants.device.h"
#endif

inline float4 get_varying_at_vertex(
    int varying_idx, int vert_idx,
    ro_vertex_buffer_t vertex_buffer
) {
    size_t idx =  vert_idx * (sizeof(vertex_shader_output_t) / sizeof(float4)) + varying_idx + 1;

    return read_vertex_buffer(vertex_buffer, idx);
}

inline float4 interpolate_varying(
    int varying_idx, const uint3 vert_idx, const float3 bary, 
    ro_vertex_buffer_t vertex_buffer
) {
    float4 v0 = get_varying_at_vertex(varying_idx, vert_idx.x, vertex_buffer);
    float4 v1 = get_varying_at_vertex(varying_idx, vert_idx.y, vertex_buffer);
    float4 v2 = get_varying_at_vertex(varying_idx, vert_idx.z, vertex_buffer);
    return v0 * bary.x + v1 * bary.y + v2 * bary.z; 
}

typedef struct {
    float4 gl_FragColor;
    float gl_FragDepth;
} fragment_shader_output_t;

#define FS_MAIN(...) \
    inline bool gl_fragment_shader( \
        global void* gl_uniforms, \
        FS_KERNEL_PARAMS \
        fragment_shader_output_t* output, \
        ro_vertex_buffer_t vertex_buffer, \
        uint3 vert_idx, float3 bary \
    ) { \
        /* Define accessible objects from fragment shader */ \
        FS_DEFINES \
        /* Set values from vertex_buffer */ \
        uint gl_uniform_offset = 0; \
        FS_SETS \
        /* Run vertex shader */ \
        __VA_ARGS__ \
        /* Fill fragment shader output */ \
        output->gl_FragColor = gl_FragColor; \
        return true; \
    }

#endif // BACKEND_SHADERS_FRAGMENT_CL
