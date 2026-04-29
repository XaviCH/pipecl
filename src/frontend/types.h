#ifndef __FRONTEND_TYPES_H__
#define __FRONTEND_TYPES_H__

#include <types.device.h>
#include <GLSC2/glsc2.h>

typedef struct {
    GLint x, y;
    GLsizei width, height;
} box_t;

typedef struct {
    float values[4]; // default: {0.0, 0.0, 0.0, 1.0} 
} vertex_attrib_t;

typedef struct {
    GLint size;
    GLenum type;
    uint16_t normalized;
    GLsizei stride;
    void *pointer;
    GLuint binding;
    cl_mem mem;
} vertex_attrib_pointer_t;

typedef struct {
    GLuint binding;
    const void *pointer;
} vertex_attribute_binding_t;

typedef union {
    vertex_attrib_t vec4;
    vertex_attrib_pointer_t pointer;
} vertex_attrib_u;

/*
typedef struct {
    unsigned int vertex_location, fragment_location, size, type, offset;
    char name[128]; 
} arg_data_t;
*/

typedef struct { 
    GLboolean last_load_attempt, last_validation_attempt;
    unsigned char log[128];
} program_data_t;

typedef struct {
    // Program data
    GLboolean               last_load_attempt; 
    GLboolean               last_validation_attempt;
    unsigned char           log[128];
    unsigned int            log_length;
    size_t                  program_id;
    // Uniform data
    size_t                  uniform_sz;
    arg_data_t              uniform_arg_datas[DEVICE_UNIFORM_CAPACITY];
    uint8_t                 uniform_data[DEVICE_UNIFORM_CAPACITY];
    // Vertex data
    size_t                  vertex_attrib_sz;
    arg_data_t              vertex_attrib_arg_datas[DEVICE_VERTEX_ATTRIBUTE_SIZE];
} gl_program_t;

typedef struct {
    cl_program triangle_setup, bin_raster, coarse_raster, force_clear;
} rasterization_program_container_t;

typedef struct {
    GLboolean depth_test, stencil_test, scissor_test, pixel_ownership, dither, blend, cull_face, polygon_offset_fill;
} enabled_container_t;

typedef struct {
    GLboolean r,g,b,a;
} color_mask_t;

typedef struct {
    uint8_t front, back;
} stencil_mask_t;

typedef struct {
    color_mask_t color;
    GLboolean depth;
    stencil_mask_t stencil;
} mask_container_t;

typedef struct
{
    uint8_t unpack_aligment;
} pixel_store_t;

typedef struct {
    GLint s, t, min_filter, mag_filter;
} texture_wraps_t;

typedef struct
{
    texture_wraps_t wraps;
    uint32_t width, height;
    uint32_t id;
    uint32_t levels;
    GLenum internalformat;
    GLboolean used; // ? maybe rm
    cl_mem mem; // TODO: rm
} gl_texture_t;

typedef struct
{
    uint32_t binding;
} active_texture_t;

typedef struct {
    GLsizeiptr size;
    GLenum usage;
    uint32_t id;
} gl_buffer_t;

typedef struct {
    GLenum target;
    uint32_t binding;
} attachment_t;

typedef struct {
    attachment_t color_attachment0, depth_attachment, stencil_attachment;
    GLboolean used;
    size_t id;
} gl_framebuffer_t;

typedef struct {
    cl_mem mem;
    GLenum internalformat;
    GLsizei width, height;
    GLboolean used;
    size_t id;
} gl_renderbuffer_t;

typedef struct { GLfloat n, f; } depth_range_t;

typedef struct { 
    GLenum func; 
    GLint ref; 
    GLuint mask; 
} stencil_function_t;

typedef struct { 
    GLenum sfail,  dpfail,  dpass; 
} stencil_operation_t;

typedef struct { 
    stencil_function_t function; 
    stencil_operation_t operation;
} stencil_face_data_t;

typedef struct { 
    stencil_face_data_t front, back; 
} stencil_data_t;

typedef struct {
    GLint left, bottom;
    GLsizei width, height; 
} scissor_data_t;

typedef struct {
    GLenum modeRGB, modeAlpha;
} blend_equation_t;

typedef struct {
    GLenum srcRGB, dstRGB, srcAlpha, dstAlpha;
} blend_func_t;

typedef struct {
    GLfloat red, green, blue, alpha;
} blend_color_t;

typedef struct {
    blend_equation_t equation;
    blend_func_t func;
    blend_color_t color;
} blend_data_t;

typedef struct {
    GLfloat red, green, blue, alpha;
} clear_color_t;

typedef struct {
    clear_color_t color;
    GLfloat depth; 
    GLint stencil;
} gl_clear_data_t;

typedef struct {
    GLboolean colorbuffer, depthbuffer, stencilbuffer;
} active_deferred_clear_t;

typedef struct {
    GLfloat factor, units;
} polygon_offset_t;

typedef struct {
    polygon_offset_t polygon_offset;
    GLenum front_face, cull_face;
    GLfloat line_width;
} rasterization_data_t;

typedef struct {
    GLenum target;
    GLenum id;
    GLenum internalformat;
} buffer_data_t; 

typedef struct {
    buffer_data_t color, depth, stencil;
    GLsizei width, height;
} framebuffer_data_t; 

typedef struct {
    GLint colorbuffer_binding, depthbuffer_binding, stencilbuffer_binding;
    GLenum colorbuffer_target, depthbuffer_target, stencilbuffer_target;

    GLboolean mask_red, mask_green, mask_blue, mask_alpha;
    GLboolean mask_depth;
    GLuint mask_stencil_front, mask_stencil_back;

    GLfloat clear_color_red, clear_color_green, clear_color_blue, clear_color_alpha;
    GLfloat clear_depth;
    GLuint clear_stencil;
} clear_state_t;

typedef struct {
    size_t num_triangles;
    size_t program_binding;
    size_t context_id;
    size_t config_count;
    GLenum draw_mode;
} draw_state_t;

typedef struct {
    size_t              framebuffer_binding;
    size_t              framebuffer_size;
    gl_framebuffer_t    framebuffers[HOST_FRAMEBUFFER_SIZE];

    size_t              renderbuffer_binding;
    size_t              renderbuffer_size;
    gl_renderbuffer_t   renderbuffers[HOST_RENDERBUFFERS_SIZE];

    size_t              texture_binding;
    size_t              texture_size;
    gl_texture_t        textures[HOST_TEXTURES_SIZE];

    size_t              buffer_binding;
    size_t              buffer_size;
    gl_buffer_t         buffers[HOST_BUFFERS_SIZE];

    size_t              active_texture;
    size_t              active_program;
    gl_program_t        programs[HOST_PROGRAMS_SIZE];
} gl_state_t;

#endif // __FRONTEND_TYPES_H__