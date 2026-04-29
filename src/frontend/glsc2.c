#include <GLSC2/glsc2.h>

#include <middleware/orchestrator.h>
#include <frontend/types.h>
#include <types.device.h>

#include <math.h>
#include <string.h>
#include <stdio.h>

// macros

#define NOT_IMPLEMENTED \
    { \
        printf("Funtion %s at %s:%d is not implemented.\n", __func__, __FILE__, __LINE__); \
        exit(1); \
    }

#ifdef NDEBUG
    #define DEBUG_THROW_GL_ERROR(_ERROR)
#else 
    #define DEBUG_THROW_GL_ERROR(_ERROR) \
    { \
        if (_ERROR != CL_SUCCESS) { \
            printf("GLSC2 driver throwed an error in function %s at %s:%d with value %d.\n", __func__, __FILE__, __LINE__, (int)_ERROR); \
            exit(_ERROR); \
        } \
    }
#endif

#define SET_ERROR_AND_RETURN(_ERROR) \
    { \
        DEBUG_THROW_GL_ERROR(_ERROR) \
        gl_error = _ERROR; \
        return; \
    }

#define SET_ERROR_AND_RETURN_VALUE(_ERROR, _VALUE) \
    { \
        DEBUG_THROW_GL_ERROR(_ERROR) \
        gl_error = _ERROR; \
        return _VALUE; \
    }

#define IS_POWER_OF_2(a) !(a & 0x1u) 

// static objects

// todo: maybe merge objects into a single one, or be less sparced with a modular aproach

GLenum gl_error = GL_NO_ERROR;

box_t               _viewport; // TODO: Checkout the default initial value

enabled_container_t _enableds = {
    .scissor_test = GL_FALSE,
    .stencil_test = GL_FALSE,
    .depth_test = GL_FALSE,
    .blend = GL_FALSE,
    .dither = GL_TRUE,
    .cull_face = GL_FALSE,
    .polygon_offset_fill = GL_FALSE,
};

mask_container_t _masks = {
    .color = { .r=GL_TRUE, .g=GL_TRUE, .b=GL_TRUE, .a=GL_TRUE },
    .depth = GL_TRUE,
    .stencil = { .back=0xFFu , .front=0xFFu },
};

pixel_store_t _pixel_store = { .unpack_aligment = 4 };

gl_clear_data_t _clear_data = {
    .color = { 0, 0, 0, 0 },
    .stencil = 0,
    .depth = 1,
};

scissor_data_t _scissor_data = {
    .left = 0,
    .bottom = 0,
    .width = 0, // TODO: If context provider, set to default framebuffer size
    .height = 0, // TODO: If context provider, set to default framebuffer size
};

stencil_data_t _stencil_data = {
    .front = {
        .function = { .func = GL_ALWAYS, .ref = 0, .mask = 1, },
        .operation = { .sfail = GL_KEEP, .dpfail = GL_KEEP, .dpass = GL_KEEP }
    },
    .back = {
        .function = { .func = GL_ALWAYS, .ref = 0, .mask = 1, },
        .operation = { .sfail = GL_KEEP, .dpfail = GL_KEEP, .dpass = GL_KEEP }
    },
};

blend_data_t _blend_data = {
    .equation = {
        .modeRGB = GL_FUNC_ADD, .modeAlpha = GL_FUNC_ADD,
    },
    .func = {
        .srcRGB = GL_ZERO, .srcAlpha = GL_ZERO,
        .dstRGB = GL_ONE, .dstAlpha = GL_ONE,
    },
    .color = {
        .red = 0, .green = 0, .blue = 0, .alpha = 0
    }
};

rasterization_data_t _rasterization_data = {
    .polygon_offset = {
        .factor = GL_ZERO,
        .units = GL_ZERO,
    },
    .cull_face = GL_BACK,
    .front_face = GL_CCW,
    .line_width = GL_ONE,
};


cl_ulong c_clear_write_values;
cl_ushort c_clear_enabled_data = 0;
active_deferred_clear_t _deferred_clear;

// Host State
static uint8_t                      vertex_attibute_updated;
static float                    vertex_attributes           [DEVICE_VERTEX_ATTRIBUTE_SIZE][4];
static vertex_attribute_data_t      vertex_attribute_datas      [DEVICE_VERTEX_ATTRIBUTE_SIZE];
static vertex_attribute_binding_t   vertex_attribute_binding    [DEVICE_VERTEX_ATTRIBUTE_SIZE];

static uint8_t                  texture_unit_updated;
static size_t                   active_texture_unit;
static size_t                   texture_unit_bindings    [DEVICE_TEXTURE_UNITS];
static size_t                   _texture_sz;
static size_t                   _texture_binding;
static gl_texture_t                _textures                [HOST_TEXTURES_SIZE];

static uint8_t                  rop_config_updated;

static uint8_t                  _uniform_data_updated;
static size_t                   _current_program;
static size_t                   _program_sz;
static gl_program_t                _programs                [HOST_PROGRAMS_SIZE];
static gl_program_t*            gl_get_current_program() { return &_programs[_current_program-1]; }

static uint32_t                 _framebuffer_updated;
static size_t                   _framebuffer_binding;
static size_t                   _framebuffer_sz;
static gl_framebuffer_t         _framebuffers   [HOST_FRAMEBUFFER_SIZE];
static gl_framebuffer_t*        gl_get_binded_framebuffer() { return &_framebuffers[_framebuffer_binding-1]; }



static size_t                   _buffer_binding;
static size_t                   _buffer_sz;
static gl_buffer_t                 _buffers        [HOST_BUFFERS_SIZE];

static size_t                   _renderbuffer_binding;
static size_t                   _renderbuffer_sz;
static gl_renderbuffer_t        _renderbuffers  [HOST_RENDERBUFFERS_SIZE];

static GLenum _hint;

static GLenum      _depth_func = GL_LESS;
static depth_range_t _depth_range = { .n=0.0, .f=1.0};

// Enqeueu State
static clear_state_t _clear_state;

// Device State
static orch_handler_t __orch;
static orch_handler_t* const orch = &__orch;

static size_t sizeof_gl_type(GLenum type);

__attribute__((constructor))
static void __context_constructor__() 
{
    orch_init(orch);

    vertex_attibute_updated = 1;
    for(size_t i = 0; i<DEVICE_VERTEX_ATTRIBUTE_SIZE; ++i) 
    {
        vertex_attributes[i][0] = 0;
        vertex_attributes[i][1] = 0;
        vertex_attributes[i][2] = 0;
        vertex_attributes[i][3] = 1;
        vertex_attribute_datas[i].misc = 0;
    }

    texture_unit_updated = 1;
    active_texture_unit = 0;
    for (size_t i = 0; i < DEVICE_TEXTURE_UNITS; ++i) 
    {
        texture_unit_bindings[i] = 0;
    }

    rop_config_updated = 1;

    _program_sz = 0;

    _framebuffer_updated = 0;
    _framebuffer_binding = 0;
    _framebuffer_sz      = 0;

    _texture_sz = 0;
    _texture_binding = 0;

    _buffer_binding = 0;
    _buffer_sz = 0;

    _renderbuffer_binding = 0;
    _renderbuffer_sz = 0;

    _hint = GL_DONT_CARE;
    gl_error = GL_NO_ERROR;
} 


/****** Interface for utils & inline functions ******\
 * Utility or inline function are implemented at the end of the file. 
*/



typedef struct { uint32_t width, height; } attachment_size_t;

// checkers
static GLboolean            is_valid_blend_equation(GLenum mode);
static GLboolean            is_valid_blend_function(GLenum mode);
static GLboolean            is_valid_stencil_operation(GLenum operation);
static GLboolean            is_valid_program(GLuint program);
static GLboolean            is_valid_tex_parameter(GLenum pname, GLint param);
static GLboolean            is_valid_face(GLenum face);
static GLboolean            is_valid_uniform_data(GLint location, size_t size, GLenum type);
static GLboolean            is_valid_arg_size_type(arg_data_t* arg_data, size_t size, GLenum type);
static GLboolean            is_valid_draw_mode(GLenum mode);
static GLboolean            is_valid_renderbuffer_internalformat(GLenum internalformat);
// getters
static attachment_size_t    get_attachment_size(attachment_t* attachment);
static uint32_t             get_clear_color(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
static uint16_t             get_clear_depth(GLfloat depth);
static uint8_t              get_clear_stencil(GLuint stencil);
static enabled_data_t            get_clear_enabled_data(GLbitfield mask);
static cl_ulong             get_clear_write_values();
static GLboolean*           get_enabled_pointer(GLenum cap);
static render_mode_t        get_render_mode(GLenum mode);
static GLint*               get_tex_parameter_pointer(GLenum pname);
static uint32_t             get_texture_mode_from_internalformat(GLenum internalformat);
static enabled_data_t       get_enabled_data_from_gl_mask();
static gl_stencil_data_t    get_stencil_data_from_gl_state();
static rop_config_t         get_rop_config(GLenum mode);
// setters
static void set_uniform_data(GLint location, size_t size, const void* data);
static void gl_set_vertex_attribute(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
static void set_stencil_operation(stencil_operation_t *stencil_operation, GLenum sfail, GLenum dfail, GLenum dpass);
static void set_stencil_function(stencil_function_t *stencil_function, GLenum func, GLint ref, GLuint mask);
// gl input parsers
static size_t       sizeof_vertex_attribute_type(GLenum type);
static GLboolean    gl_tex_parameter_to_tex_wrap(GLint param, uint32_t* result);
static uint32_t     gl_blend_equation_to_blend_equation(GLenum equation);
static uint32_t     gl_blend_func_to_blend_func(GLenum func);
static uint32_t     gl_blend_equation_to_blend_equation(GLenum equation);
static uint32_t     gl_depth_func_to_depth_func(GLenum func);
static uint32_t     gl_stencil_func_to_stencil_func(GLenum func);
static uint32_t     gl_stencil_op_to_stencil_op(GLenum op);
static uint32_t     gl_tex_filter_to_tex_filter(GLint param);
static uint32_t     gl_tex_wrap_to_tex_wrap(GLint param);

static uint32_t     rgba_to_uint32(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
static GLboolean    gl_size_to_vertex_attribute_size(GLint gl_size, unsigned int* va_size);
static GLboolean    gl_type_to_vertex_attribute_type(GLenum gl_type, unsigned int* va_type);
// 
static void                 update_colorbuffer();
static void                 update_depthbuffer();
static void                 update_stencilbuffer();
static void                 update_device_framebuffer();


// TODO: Dependant texture throws this if (id >= MAX_COMBINED_TEXTURE_IMAGE_UNITS) SET_ERROR_AND_RETURN(GL_INVALID_OPERATION);
GL_APICALL void GL_APIENTRY glActiveTexture (GLenum texture) 
{
    uint32_t texture_unit = texture - GL_TEXTURE0;

    if (texture_unit >= DEVICE_TEXTURE_UNITS) SET_ERROR_AND_RETURN(GL_INVALID_VALUE); // TODO: MAX_COMBINED_TEXTURE_IMAGE_UNITS

    active_texture_unit = texture_unit;
    _texture_binding = texture_unit_bindings[active_texture_unit];
}

GL_APICALL void GL_APIENTRY glBindBuffer (GLenum target, GLuint buffer) 
{
    if (target != GL_ARRAY_BUFFER) SET_ERROR_AND_RETURN(GL_INVALID_ENUM);

    if (buffer > _buffer_sz) SET_ERROR_AND_RETURN(GL_INVALID_OPERATION);

    _buffer_binding = buffer;
}

GL_APICALL void GL_APIENTRY glBindFramebuffer (GLenum target, GLuint framebuffer) 
{
    if (target != GL_FRAMEBUFFER) SET_ERROR_AND_RETURN(GL_INVALID_ENUM);

    if (framebuffer > _framebuffer_sz) SET_ERROR_AND_RETURN(GL_INVALID_OPERATION);

    _framebuffer_updated = 1;
    _framebuffer_binding = framebuffer;
}

GL_APICALL void GL_APIENTRY glBindRenderbuffer (GLenum target, GLuint renderbuffer) 
{
    if (target != GL_RENDERBUFFER) SET_ERROR_AND_RETURN(GL_INVALID_ENUM);

    if (renderbuffer > _renderbuffer_sz) SET_ERROR_AND_RETURN(GL_INVALID_OPERATION);

    _renderbuffer_binding = renderbuffer;
}

GL_APICALL void GL_APIENTRY glBindTexture (GLenum target, GLuint texture) 
{
    if (target != GL_TEXTURE_2D) SET_ERROR_AND_RETURN(GL_INVALID_ENUM);

    if (texture > _texture_sz) SET_ERROR_AND_RETURN(GL_INVALID_OPERATION);

    texture_unit_bindings[active_texture_unit] = texture;
    _texture_binding = texture;

    texture_unit_updated = 1;
}

GL_APICALL void GL_APIENTRY glBlendColor (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) 
{
    _blend_data.color = (blend_color_t) 
    {
        .red = red,
        .green = green,
        .blue = blue,
        .alpha = alpha,
    };
}

GL_APICALL void GL_APIENTRY glBlendEquation (GLenum mode) 
{
    glBlendEquationSeparate(mode, mode);
}

GL_APICALL void GL_APIENTRY glBlendEquationSeparate (GLenum modeRGB, GLenum modeAlpha) 
{
    if (is_valid_blend_equation(modeRGB) == GL_FALSE) SET_ERROR_AND_RETURN(GL_INVALID_ENUM);

    if (is_valid_blend_equation(modeAlpha) == GL_FALSE) SET_ERROR_AND_RETURN(GL_INVALID_ENUM);
    
    _blend_data.equation = (blend_equation_t) 
    {
        .modeRGB    = modeRGB,
        .modeAlpha  = modeAlpha,
    };

    rop_config_updated = 1;
}

GL_APICALL void GL_APIENTRY glBlendFunc (GLenum sfactor, GLenum dfactor) 
{
    glBlendFuncSeparate(sfactor, dfactor, sfactor, dfactor);

    rop_config_updated = 1;
}

GL_APICALL void GL_APIENTRY glBlendFuncSeparate (GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha) 
{
    if (is_valid_blend_function(sfactorRGB) == GL_FALSE) SET_ERROR_AND_RETURN(GL_INVALID_ENUM);

    if (is_valid_blend_function(dfactorRGB) == GL_FALSE) SET_ERROR_AND_RETURN(GL_INVALID_ENUM);
    
    if (is_valid_blend_function(sfactorAlpha) == GL_FALSE) SET_ERROR_AND_RETURN(GL_INVALID_ENUM);
    
    if (is_valid_blend_function(dfactorAlpha) == GL_FALSE) SET_ERROR_AND_RETURN(GL_INVALID_ENUM);
    
    _blend_data.func = (blend_func_t)
    {
        .srcRGB     = sfactorRGB, 
        .srcAlpha   = sfactorAlpha,
        .dstRGB     = dfactorRGB, 
        .dstAlpha   = dfactorAlpha,
    };

    rop_config_updated = 1;
}

GL_APICALL void GL_APIENTRY glBufferData (GLenum target, GLsizeiptr size, const void *data, GLenum usage) 
{
    if (target != GL_ARRAY_BUFFER) SET_ERROR_AND_RETURN(GL_INVALID_ENUM);

    if (_buffer_binding == 0) SET_ERROR_AND_RETURN(GL_INVALID_OPERATION);

    if (size == 0) SET_ERROR_AND_RETURN(GL_INVALID_VALUE); // TODO: Is size==0 valid?

    gl_buffer_t* buffer = &_buffers[_buffer_binding-1];
    uint32_t is_inizialized = buffer->size != 0;

    if (is_inizialized) 
    {
        if (buffer->size != size || buffer->usage != usage) SET_ERROR_AND_RETURN(GL_INVALID_OPERATION);
    }
    else 
    {
        buffer->size = size;
        buffer->usage = usage;
        buffer->id = orch_create_buffer(orch, size);
        // TODO: Set CL_OUT_OF_RESOURCES if no memory available
    }

    if (data != NULL)
    {
        orch_write_buffer(orch, buffer->id, 0, size, data);
    }
}

GL_APICALL void GL_APIENTRY glBufferSubData (GLenum target, GLintptr offset, GLsizeiptr size, const void *data) 
{
    if (target != GL_ARRAY_BUFFER) SET_ERROR_AND_RETURN(GL_INVALID_ENUM);

    if (_buffer_binding == 0) SET_ERROR_AND_RETURN(GL_INVALID_OPERATION);

    if (offset < 0 || size < 0 ) SET_ERROR_AND_RETURN(GL_INVALID_VALUE);

    gl_buffer_t *buffer = &_buffers[_buffer_binding-1];

    if (offset + size > buffer->size) SET_ERROR_AND_RETURN(GL_INVALID_VALUE);

    orch_write_buffer(orch, buffer->id, offset, size, data);
}

static attachment_size_t get_any_attachment() 
{
    gl_framebuffer_t *framebuffer = gl_get_binded_framebuffer();

    if (framebuffer->color_attachment0.binding) 
    {
        return get_attachment_size(&framebuffer->color_attachment0);
    }

    if (framebuffer->depth_attachment.binding) 
    {
        return get_attachment_size(&framebuffer->depth_attachment);
    }

    if (framebuffer->stencil_attachment.binding) 
    {
        return get_attachment_size(&framebuffer->stencil_attachment);
    }
}

GL_APICALL GLenum GL_APIENTRY glCheckFramebufferStatus (GLenum target) 
{
    if (target != GL_FRAMEBUFFER) SET_ERROR_AND_RETURN_VALUE(GL_INVALID_ENUM, 0);

    if (_framebuffer_binding == 0) NOT_IMPLEMENTED;

    gl_framebuffer_t *framebuffer = gl_get_binded_framebuffer();
    
    uint32_t is_color_attached = framebuffer->color_attachment0.binding;
    uint32_t is_depth_attached = framebuffer->depth_attachment.binding;
    uint32_t is_stencil_attached = framebuffer->stencil_attachment.binding;
    
    uint32_t any_attachment = 
        is_color_attached   ||
        is_depth_attached   ||
        is_stencil_attached ;

    if (!any_attachment) return GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;

    attachment_size_t common_size = {0,0};
    attachment_size_t tmp_size = {0,0};

    if (is_color_attached) 
    {
        common_size = get_attachment_size(&framebuffer->color_attachment0);
    }

    if (is_depth_attached) 
    {
        tmp_size = get_attachment_size(&framebuffer->depth_attachment);
        if (common_size.width == 0 && common_size.height == 0) 
        {
            common_size = tmp_size;
        } 
        else if (common_size.width != tmp_size.width || common_size.height != tmp_size.height) 
        {
            return GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS;
        }
    }

    if (is_stencil_attached) 
    {
        tmp_size = get_attachment_size(&framebuffer->stencil_attachment);
        if (common_size.width == 0 && common_size.height == 0) 
        {
            common_size = tmp_size;
        } 
        else if (common_size.width != tmp_size.width || common_size.height != tmp_size.height) 
        {
            return GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS;
        }
    }

    return GL_FRAMEBUFFER_COMPLETE;
}

static clear_data_t get_clear_data_from_gl_state()
{
    clear_data_t clear_data;

    clear_data.color.misc = get_clear_color(
        _clear_state.clear_color_red,
        _clear_state.clear_color_green,
        _clear_state.clear_color_blue,
        _clear_state.clear_color_alpha
    );

    clear_data.depth.misc = get_clear_depth(
        _clear_state.clear_depth
    );

    clear_data.stencil.misc = get_clear_stencil(
        _clear_state.clear_stencil
    );

    return clear_data;
}

GL_APICALL void GL_APIENTRY glClear (GLbitfield mask) 
{
    GLbitfield valid_mask = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;

    if (mask & valid_mask == 0) SET_ERROR_AND_RETURN(GL_INVALID_VALUE);
    
    if (_framebuffer_binding > _framebuffer_sz) SET_ERROR_AND_RETURN(GL_INVALID_OPERATION);

    if (_framebuffer_binding == 0) SET_ERROR_AND_RETURN(GL_INVALID_OPERATION); // TODO: Not implemented

    gl_framebuffer_t *framebuffer = gl_get_binded_framebuffer();
    
    clear_data_t clear_data = (clear_data_t) {
        .color.misc = get_clear_color(
            _clear_data.color.red, 
            _clear_data.color.green, 
            _clear_data.color.blue, 
            _clear_data.color.alpha
        ),
        .depth.misc = get_clear_depth(_clear_data.depth),
        .stencil = get_clear_stencil(_clear_data.stencil)
    };

    enabled_data_t enabled_data = get_clear_enabled_data(mask);

    orch_clear(
        orch,
        framebuffer->id, 
        clear_data, 
        enabled_data
    );
}

GL_APICALL void GL_APIENTRY glClearColor (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) 
{
    _clear_data.color = (clear_color_t) {
        .red = red,
        .green = green,
        .blue = blue,
        .alpha = alpha
    };
}

GL_APICALL void GL_APIENTRY glClearDepthf (GLfloat d) 
{
    _clear_data.depth = d;
}

GL_APICALL void GL_APIENTRY glClearStencil (GLint s) 
{
    _clear_data.stencil = s;
}

GL_APICALL void GL_APIENTRY glColorMask (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) 
{
    _masks.color = (color_mask_t) {
        .r = red,
        .g = green,
        .b = blue,
        .a = alpha,
    };
    rop_config_updated = 1;
}

GL_APICALL void GL_APIENTRY glCompressedTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data)
{
    NOT_IMPLEMENTED;
}

GL_APICALL GLuint GL_APIENTRY glCreateProgram (void)
{
    _program_sz += 1;
    
    return _program_sz;
}

GL_APICALL void GL_APIENTRY glCullFace (GLenum mode) 
{
    switch (mode)
    {
        default: 
            SET_ERROR_AND_RETURN(GL_INVALID_ENUM);
        case GL_FRONT:
        case GL_BACK:
        case GL_FRONT_AND_BACK:
            ;
    }

    _rasterization_data.cull_face = mode;

    rop_config_updated = 1;
}

GL_APICALL void GL_APIENTRY glDepthFunc (GLenum func) 
{
    switch (func)
    {
        default:            
            SET_ERROR_AND_RETURN(GL_INVALID_ENUM);
        case GL_NEVER:
        case GL_LESS:
        case GL_EQUAL:
        case GL_LEQUAL:
        case GL_GREATER:
        case GL_NOTEQUAL:
        case GL_GEQUAL:
        case GL_ALWAYS:
            ;
    }

    _depth_func = func;
    rop_config_updated = 1;
}

GL_APICALL void GL_APIENTRY glDepthMask (GLboolean flag) 
{
    _masks.depth = flag;
    rop_config_updated = 1;
}

GL_APICALL void GL_APIENTRY glDepthRangef (GLfloat n, GLfloat f) 
{
    _depth_range = (depth_range_t) 
    {
        .n=n,
        .f=f
    };
    rop_config_updated = 1;
}

GL_APICALL void GL_APIENTRY glDisable (GLenum cap) 
{
    GLboolean* enabled_ptr = get_enabled_pointer(cap);

    if (enabled_ptr == NULL) SET_ERROR_AND_RETURN(GL_INVALID_ENUM);

    *enabled_ptr = GL_FALSE;
    rop_config_updated = 1;
}

GL_APICALL void GL_APIENTRY glDisableVertexAttribArray (GLuint index) 
{
    if (index >= GL_MAX_VERTEX_ATTRIBS) SET_ERROR_AND_RETURN(GL_INVALID_VALUE);

    vertex_attibute_updated = 1;
    vertex_attribute_datas[index].misc &= ~VERTEX_ATTRIBUTE_ACTIVE_POINTER;
}

static void write_gl_state_to_orch(GLenum draw_mode)
{
    gl_framebuffer_t* framebuffer = gl_get_binded_framebuffer();

    if (vertex_attibute_updated)
    {
        orch_write_vertex_attributes(orch, framebuffer->id, vertex_attributes);

        orch_write_vertex_attribute_data(orch, framebuffer->id, vertex_attribute_datas);

        for (size_t attribute=0; attribute < DEVICE_VERTEX_ATTRIBUTE_SIZE; ++attribute) 
        {
            vertex_attribute_binding_t *va_binding = &vertex_attribute_binding[attribute]; 
            if (va_binding->binding != 0) 
            {
                gl_buffer_t *buffer = &_buffers[va_binding->binding-1];
                orch_attach_vertex_attribute_ptr(orch, framebuffer->id, attribute, buffer->id);
            } 
            else if (va_binding->pointer != NULL)
            {
                vertex_attribute_data_t *va_data = &vertex_attribute_datas[attribute];
                orch_attach_vertex_attribute_host_ptr(orch, framebuffer->id, attribute, va_data->stride, (void*) va_binding->pointer);
            } 
            else
            {
                orch_attach_vertex_attribute_ptr(orch, framebuffer->id, attribute, 0);
            }
        }
        vertex_attibute_updated = 0;
    }

    if (texture_unit_updated) 
    {
        texture_data_t texture_datas[DEVICE_TEXTURE_UNITS];

        for (size_t i = 0; i < DEVICE_TEXTURE_UNITS; ++i)
        {
            if (texture_unit_bindings[i] == 0) continue;

            gl_texture_t *texture = &_textures[texture_unit_bindings[i]-1];

            texture_data_t *texture_data = &texture_datas[i];

            set_texture_data_mode(
                texture_data, 
                get_texture_mode_from_internalformat(texture->internalformat)
            );

            set_texture_data_mag_filter(
                texture_data,
                gl_tex_filter_to_tex_filter(texture->wraps.mag_filter)
            );

            set_texture_data_min_filter(
                texture_data, 
                gl_tex_filter_to_tex_filter(texture->wraps.min_filter)
            );

            set_texture_data_wrap_s(
                texture_data, 
                gl_tex_wrap_to_tex_wrap(texture->wraps.s)
            );

            set_texture_data_wrap_t(
                texture_data, 
                gl_tex_wrap_to_tex_wrap(texture->wraps.t)
            );

            set_texture_data_size(
                texture_data,
                (texture_size_t){texture->width, texture->height}
            );
        }

        orch_write_fragment_texture_data(orch, framebuffer->id, texture_datas);

        for (size_t unit = 0; unit < DEVICE_TEXTURE_UNITS; ++unit)
        {
            size_t binding = texture_unit_bindings[unit];
            if (binding == 0)
            {
                orch_attach_texture_unit(orch, framebuffer->id, unit, 0);
            }
            else
            {
                gl_texture_t *texture = &_textures[binding-1];
                orch_attach_texture_unit(orch, framebuffer->id, unit, texture->id);
            }

        }
        texture_unit_updated = 0;
    }
    
    if (rop_config_updated || _uniform_data_updated)
    {
        rop_config_t rop_config = get_rop_config(draw_mode);
        orch_write_fragment_data(orch, framebuffer->id, _programs[_current_program-1].uniform_data, rop_config);
        rop_config_updated = 0;
        _uniform_data_updated = 0;
    }
}

GL_APICALL void GL_APIENTRY glDrawArrays (GLenum mode, GLint first, GLsizei count) 
{
    if (_current_program == 0) SET_ERROR_AND_RETURN(GL_INVALID_OPERATION);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) SET_ERROR_AND_RETURN(GL_INVALID_FRAMEBUFFER_OPERATION);

    if (first < 0) SET_ERROR_AND_RETURN(GL_INVALID_VALUE);

    if (first != 0) NOT_IMPLEMENTED;

    if (mode == GL_POINTS || mode == GL_LINE_STRIP || mode == GL_LINE_LOOP || mode == GL_LINES) NOT_IMPLEMENTED;

    gl_framebuffer_t*   framebuffer = gl_get_binded_framebuffer();
    gl_program_t*       program     = gl_get_current_program();
    
    render_mode_t       render_mode = get_render_mode(mode);

    write_gl_state_to_orch(mode);

    orch_draw_arrays(orch, framebuffer->id, program->program_id, render_mode, first, count);
}

GL_APICALL void GL_APIENTRY glDrawRangeElements (GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices)
{
    if (_current_program == 0) SET_ERROR_AND_RETURN(GL_INVALID_OPERATION);

    if (is_valid_draw_mode(mode) == GL_FALSE) SET_ERROR_AND_RETURN(GL_INVALID_ENUM);

    if (count < 0) SET_ERROR_AND_RETURN(GL_INVALID_VALUE);
    
    if (end < start) SET_ERROR_AND_RETURN(GL_INVALID_VALUE);
    
    if (type != GL_UNSIGNED_SHORT) SET_ERROR_AND_RETURN(GL_INVALID_ENUM);

    if (start != 0) NOT_IMPLEMENTED;
    
    if (count == 0) return;

    gl_framebuffer_t* framebuffer = gl_get_binded_framebuffer();
    gl_program_t* program = gl_get_current_program();
    render_mode_t render_mode = get_render_mode(mode);

    write_gl_state_to_orch(mode);

    orch_draw_range(orch, framebuffer->id, program->program_id, render_mode, start, end, count, (const uint16_t*) indices);
}

GL_APICALL void GL_APIENTRY glEnable (GLenum cap) 
{
    GLboolean* enabled_ptr = get_enabled_pointer(cap);

    if (enabled_ptr == NULL) SET_ERROR_AND_RETURN(GL_INVALID_ENUM);

    *enabled_ptr = GL_TRUE;
}

GL_APICALL void GL_APIENTRY glEnableVertexAttribArray (GLuint index) 
{
    if (index >= DEVICE_VERTEX_ATTRIBUTE_SIZE) SET_ERROR_AND_RETURN(GL_INVALID_VALUE);
    
    vertex_attibute_updated = 1;
    vertex_attribute_datas[index].misc |= VERTEX_ATTRIBUTE_ACTIVE_POINTER;
}

GL_APICALL void GL_APIENTRY glFinish (void) 
{
    if (_framebuffer_binding == 0) SET_ERROR_AND_RETURN(GL_INVALID_OPERATION);

    gl_framebuffer_t* framebuffer = gl_get_binded_framebuffer();

    orch_finish(orch, framebuffer->id);
}

GL_APICALL void GL_APIENTRY glFlush (void) 
{
    if (_framebuffer_binding == 0) SET_ERROR_AND_RETURN(GL_INVALID_OPERATION);

    gl_framebuffer_t* framebuffer = gl_get_binded_framebuffer();

    orch_flush(orch, framebuffer->id);
}

GL_APICALL void GL_APIENTRY glFramebufferRenderbuffer (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) 
{
    if (target != GL_FRAMEBUFFER) SET_ERROR_AND_RETURN(GL_INVALID_ENUM);

    if (_framebuffer_binding == 0) SET_ERROR_AND_RETURN(GL_INVALID_OPERATION);

    gl_framebuffer_t *framebuffer = gl_get_binded_framebuffer();

    attachment_t *attachment_ptr;

    switch (attachment)
    {
    case GL_COLOR_ATTACHMENT0:
        attachment_ptr = &framebuffer->color_attachment0;
        break;
    case GL_DEPTH_ATTACHMENT: 
        attachment_ptr = &framebuffer->depth_attachment;
        break;
    case GL_STENCIL_ATTACHMENT: 
        attachment_ptr = &framebuffer->stencil_attachment;
        break;
    default:
        SET_ERROR_AND_RETURN(GL_INVALID_ENUM);
    }

    gl_renderbuffer_t* renderbuffer_ptr;
    size_t renderbuffer_id = 0;

    if (renderbuffer)
    {
        if (renderbuffertarget != GL_RENDERBUFFER) SET_ERROR_AND_RETURN(GL_INVALID_ENUM);

        if (renderbuffer > _renderbuffer_sz) SET_ERROR_AND_RETURN(GL_INVALID_OPERATION);

        renderbuffer_id = _renderbuffers[renderbuffer-1].id;
    }
    
    switch (attachment)
    {
        default:
        case GL_COLOR_ATTACHMENT0:
            orch_attach_render_colorbuffer(orch, framebuffer->id, renderbuffer_id);
            break;
        case GL_DEPTH_ATTACHMENT: 
            orch_attach_render_depthbuffer(orch, framebuffer->id, renderbuffer_id);
            break;
        case GL_STENCIL_ATTACHMENT: 
            orch_attach_render_stencilbuffer(orch, framebuffer->id, renderbuffer_id);
            break;
    }

    *attachment_ptr = (attachment_t) {
        .target = renderbuffertarget,
        .binding = renderbuffer,
    };

    _framebuffer_updated = 1;
}

GL_APICALL void GL_APIENTRY glFramebufferTexture2D (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) 
{
    if (target != GL_FRAMEBUFFER) NOT_IMPLEMENTED;

    if (_framebuffer_binding == 0) SET_ERROR_AND_RETURN(GL_INVALID_OPERATION);

    if (level != 0) SET_ERROR_AND_RETURN(GL_INVALID_VALUE);

    gl_framebuffer_t *framebuffer = gl_get_binded_framebuffer();
    
    attachment_t *attachment_ptr;

    switch (attachment)
    {
        case GL_COLOR_ATTACHMENT0:
            attachment_ptr = &framebuffer->color_attachment0;
            break;
        case GL_DEPTH_ATTACHMENT:
            attachment_ptr = &framebuffer->depth_attachment;
            break;
        case GL_STENCIL_ATTACHMENT:
            attachment_ptr = &framebuffer->stencil_attachment;
            break;
        default:
            SET_ERROR_AND_RETURN(GL_INVALID_ENUM);
    }

    gl_texture_t* texture_ptr;
    size_t texture_id = 0;

    if (texture)
    {
        if (textarget != GL_TEXTURE_2D) SET_ERROR_AND_RETURN(GL_INVALID_ENUM);

        if (texture > _texture_sz) SET_ERROR_AND_RETURN(GL_INVALID_OPERATION);

        texture_id = _textures[texture-1].id;
    }

    switch (attachment)
    {
        default:
        case GL_COLOR_ATTACHMENT0:
            orch_attach_texture_colorbuffer(orch, framebuffer->id, texture_id);
            break;
        case GL_DEPTH_ATTACHMENT: 
            orch_attach_texture_depthbuffer(orch, framebuffer->id, texture_id);
            break;
        case GL_STENCIL_ATTACHMENT: 
            orch_attach_texture_stencilbuffer(orch, framebuffer->id, texture_id);
            break;
    }

    *attachment_ptr = (attachment_t) 
    {
        .target = textarget,
        .binding = texture,
    };

    _framebuffer_updated = 1;
}

GL_APICALL void GL_APIENTRY glFrontFace (GLenum mode) {
    switch (mode)
    {
    case GL_CCW:
    case GL_CW:
        _rasterization_data.front_face = mode;
        return;
    default:
        SET_ERROR_AND_RETURN(GL_INVALID_ENUM); // TODO: Checkout error
    }
}

GL_APICALL void GL_APIENTRY glGenBuffers (GLsizei n, GLuint *buffers)
{
    for (size_t i=0; i<n && _buffer_sz < HOST_BUFFERS_SIZE; ++i) 
    {
        _buffers[_buffer_sz].size = 0;

        buffers[i] = _buffer_sz + 1;
        
        _buffer_sz += 1;
    }
}

GL_APICALL void GL_APIENTRY glGenerateMipmap (GLenum target) NOT_IMPLEMENTED;

GL_APICALL void GL_APIENTRY glGenFramebuffers (GLsizei n, GLuint *framebuffers) 
{
    for (size_t i=0; i<n && _framebuffer_sz < HOST_FRAMEBUFFER_SIZE; ++i)
    {
        _framebuffers[_framebuffer_sz] = (gl_framebuffer_t) {
            .color_attachment0 = {
                .binding = 0,
                .target = 0,
            },
            .depth_attachment = {
                .binding = 0,
                .target = 0,
            },
            .stencil_attachment = {
                .binding = 0,
                .target = 0,
            },
            .id = orch_create_framebuffer(orch)
        };

        framebuffers[i] = _framebuffer_sz + 1; // framebuffer id start at 1
        _framebuffer_sz += 1;
    }
}

GL_APICALL void GL_APIENTRY glGenRenderbuffers (GLsizei n, GLuint *renderbuffers)
{
    for (size_t i=0; i<n && _renderbuffer_sz < HOST_RENDERBUFFERS_SIZE; ++i) 
    {
        gl_renderbuffer_t *renderbuffer = &_renderbuffers[_renderbuffer_sz];

        renderbuffer->width = 0;
        renderbuffer->height = 0;

        renderbuffers[i] = _renderbuffer_sz + 1;
        _renderbuffer_sz += 1;
    }
}

GL_APICALL void GL_APIENTRY glGenTextures (GLsizei n, GLuint *textures) 
{
    for (size_t i=0; i<n && _texture_sz < HOST_TEXTURES_SIZE; ++i) 
    {
        _textures[_texture_sz] = (gl_texture_t) {
            .wraps = (texture_wraps_t) {
                .s = TEXTURE_WRAP_REPEAT,
                .t = TEXTURE_WRAP_REPEAT,
                .min_filter = TEXTURE_FILTER_NEAREST,
                .mag_filter = TEXTURE_FILTER_NEAREST,
            },
            .width = 0, 
            .height = 0,
        };

        textures[i] = _texture_sz + 1; // texture id start at 1
        _texture_sz += 1;
    }
}

GL_APICALL GLint GL_APIENTRY glGetAttribLocation (GLuint program, const GLchar *name) 
{
    if (program == 0) SET_ERROR_AND_RETURN_VALUE(GL_INVALID_OPERATION, -1);

    gl_program_t* program_ptr = &_programs[program-1];

    // TOOD: if (!program_ptr->last_load_attempt) SET_ERROR_AND_RETURN(GL_INVALID_OPERATION);
    
    for(size_t attrib=0; attrib<program_ptr->vertex_attrib_sz; ++attrib) 
    {
        if (strcmp(name, program_ptr->vertex_attrib_arg_datas[attrib].name) == 0) return attrib;
    }

    return -1;
}

GL_APICALL void GL_APIENTRY glGetBooleanv (GLenum pname, GLboolean *data) 
{
    switch (pname)
    {
    case GL_DEPTH_WRITEMASK:
        *data = _masks.depth;
        return;
    default:
        SET_ERROR_AND_RETURN(GL_INVALID_ENUM);
    }
}

GL_APICALL void GL_APIENTRY glGetBufferParameteriv (GLenum target, GLenum pname, GLint *params) {
    NOT_IMPLEMENTED;
}

GL_APICALL GLenum GL_APIENTRY glGetError (void) 
{
    GLenum error = gl_error;
    gl_error = GL_NO_ERROR;
    return error;
}

GL_APICALL void GL_APIENTRY glGetFloatv (GLenum pname, GLfloat *data) {
    NOT_IMPLEMENTED;
}

GL_APICALL void GL_APIENTRY glGetFramebufferAttachmentParameteriv (GLenum target, GLenum attachment, GLenum pname, GLint *params) {
    NOT_IMPLEMENTED;
}

GL_APICALL GLenum GL_APIENTRY glGetGraphicsResetStatus (void) {
    NOT_IMPLEMENTED;
}

GL_APICALL void GL_APIENTRY glGetIntegerv (GLenum pname, GLint *data) 
{
    switch (pname)
    {
    case GL_IMPLEMENTATION_COLOR_READ_FORMAT:
        *data = GL_RGBA;
        return;
    case GL_IMPLEMENTATION_COLOR_READ_TYPE:
        *data = GL_UNSIGNED_SHORT_4_4_4_4;
        return;
    case GL_FRAMEBUFFER_BINDING: 
        *data = _framebuffer_binding; 
        return;
    case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:
        *data = DEVICE_VERTEX_TEXTURE_UNITS;
        return;
    case GL_MAX_TEXTURE_IMAGE_UNITS:
        *data = DEVICE_TEXTURE_UNITS;
        return;
    case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
        *data = DEVICE_VERTEX_TEXTURE_UNITS + DEVICE_TEXTURE_UNITS;
        return;
    case GL_DEPTH_FUNC:
        *data = _depth_func;
        return;
    default:
        SET_ERROR_AND_RETURN(GL_INVALID_ENUM);
    }
}

GL_APICALL void GL_APIENTRY glGetProgramiv (GLuint program, GLenum pname, GLint *params) 
{
    if (is_valid_program(program) == GL_FALSE) SET_ERROR_AND_RETURN(GL_INVALID_OPERATION);

    switch (pname)
    {
    case GL_LINK_STATUS:
        *params = _programs[program-1].last_load_attempt;
        break;
    default:
        SET_ERROR_AND_RETURN(GL_INVALID_ENUM);
    }
}

GL_APICALL void GL_APIENTRY glGetRenderbufferParameteriv (GLenum target, GLenum pname, GLint *params) {
    NOT_IMPLEMENTED;
}

GL_APICALL const GLubyte *GL_APIENTRY glGetString (GLenum name) {
    NOT_IMPLEMENTED;
}

GL_APICALL void GL_APIENTRY glGetTexParameterfv (GLenum target, GLenum pname, GLfloat *params) 
{
    if (target != GL_TEXTURE_2D) SET_ERROR_AND_RETURN(GL_INVALID_ENUM);

    if (_texture_binding == 0) SET_ERROR_AND_RETURN(GL_INVALID_OPERATION);

    gl_texture_t* texture = &_textures[_texture_binding-1];

    NOT_IMPLEMENTED;
}

GL_APICALL void GL_APIENTRY glGetTexParameteriv (GLenum target, GLenum pname, GLint *params) 
{
    if (target != GL_TEXTURE_2D) SET_ERROR_AND_RETURN(GL_INVALID_ENUM);

    if (_texture_binding == 0) SET_ERROR_AND_RETURN(GL_INVALID_OPERATION);

    gl_texture_t* texture = &_textures[_texture_binding-1];

    switch (target)
    {
    case GL_TEXTURE_IMMUTABLE_FORMAT:
        *params = texture->height && texture->width ? GL_TRUE : GL_FALSE;
        break;
    default:
        NOT_IMPLEMENTED;
    }
}

GL_APICALL void GL_APIENTRY glGetnUniformfv (GLuint program, GLint location, GLsizei bufSize, GLfloat *params) {
    NOT_IMPLEMENTED;
}

GL_APICALL void GL_APIENTRY glGetnUniformiv (GLuint program, GLint location, GLsizei bufSize, GLint *params) {
    NOT_IMPLEMENTED;
}

GL_APICALL GLint GL_APIENTRY glGetUniformLocation (GLuint program, const GLchar *name) 
{
    if (is_valid_program(program) == GL_FALSE) SET_ERROR_AND_RETURN_VALUE(GL_INVALID_OPERATION, -1);

    gl_program_t *program_ptr = &_programs[program-1];

    // TOOD: if (!program_ptr->last_load_attempt) SET_ERROR_AND_RETURN(GL_INVALID_OPERATION);

    for(size_t uniform=0; uniform < program_ptr->uniform_sz; ++uniform) {
        if (strcmp(name, program_ptr->uniform_arg_datas[uniform].name) == 0) return uniform;
    }

    return -1;
}

GL_APICALL void GL_APIENTRY glGetVertexAttribfv (GLuint index, GLenum pname, GLfloat *params) {
    NOT_IMPLEMENTED;
}

GL_APICALL void GL_APIENTRY glGetVertexAttribiv (GLuint index, GLenum pname, GLint *params) {
    NOT_IMPLEMENTED;
}

GL_APICALL void GL_APIENTRY glGetVertexAttribPointerv (GLuint index, GLenum pname, void **pointer) {
    NOT_IMPLEMENTED;
}


GL_APICALL void GL_APIENTRY glHint (GLenum target, GLenum mode) 
{
    if (target != GL_GENERATE_MIPMAP_HINT) SET_ERROR_AND_RETURN(GL_INVALID_ENUM);

    switch (mode)
    {
    default:
        SET_ERROR_AND_RETURN(GL_INVALID_ENUM);
    case GL_FASTEST:
    case GL_NICEST:
    case GL_DONT_CARE:
        ;
    }

    _hint = mode;
}

GL_APICALL GLboolean GL_APIENTRY glIsEnabled (GLenum cap) 
{
    GLboolean *enabled_ptr = get_enabled_pointer(cap);

    if (enabled_ptr == NULL) SET_ERROR_AND_RETURN_VALUE(GL_INVALID_ENUM, GL_FALSE);

    return *enabled_ptr;
}

GL_APICALL void GL_APIENTRY glLineWidth (GLfloat width) 
{
    if (width <= (GLfloat) 0) SET_ERROR_AND_RETURN(GL_INVALID_VALUE);

    _rasterization_data.line_width = width;
};

GL_APICALL void GL_APIENTRY glPixelStorei (GLenum pname, GLint param) 
{
    switch (pname)
    {
        default:
            SET_ERROR_AND_RETURN(GL_INVALID_ENUM);
        case GL_UNPACK_ALIGNMENT:
            switch (param)
            {
                default:
                    SET_ERROR_AND_RETURN(GL_INVALID_VALUE);
                case 1:
                case 2:
                case 4:
                case 8:
                    _pixel_store.unpack_aligment = param;
            }
    }
}

GL_APICALL void GL_APIENTRY glPolygonOffset (GLfloat factor, GLfloat units) 
{
    _rasterization_data.polygon_offset = (polygon_offset_t) {
        .factor = factor,
        .units = units
    };
}

GL_APICALL void GL_APIENTRY glProgramBinary (GLuint program, GLenum binaryFormat, const void *binary, GLsizei length)
{
    if (!is_valid_program(program)) SET_ERROR_AND_RETURN(GL_INVALID_OPERATION);

    gl_program_t *program_ptr = &_programs[program-1];

    program_ptr->program_id = orch_create_shader_from_binary(orch, length, binary);
    program_ptr->uniform_sz = orch_get_shader_uniform_size(orch, program_ptr->program_id);

    for(size_t i=0; i<program_ptr->uniform_sz; ++i) 
    {
        arg_data_t *arg_data = &program_ptr->uniform_arg_datas[i];
        orch_get_shader_uniform_arg_data(orch, program_ptr->program_id, i, arg_data);
        if (i == 0) 
        { 
            arg_data->offset = 0;
        }
        else 
        {
            size_t arg_size = program_ptr->uniform_arg_datas[i-1].size * sizeof_gl_type(program_ptr->uniform_arg_datas[i-1].type);
            arg_data->offset = program_ptr->uniform_arg_datas[i-1].offset + arg_size;
        }
    }

    program_ptr->vertex_attrib_sz = orch_get_shader_attribute_size(orch, program_ptr->program_id);

    for(size_t i=0; i<program_ptr->vertex_attrib_sz; ++i) 
    {
        orch_get_shader_attribute_arg_data(orch, program_ptr->program_id, i, &program_ptr->vertex_attrib_arg_datas[i]);
    }
}

static uint32_t gl_get_texture_mode_from_format_type(GLenum format, GLenum type)
{
    // TODO: do all cases
    if (type == GL_UNSIGNED_BYTE)
    {
        switch (format)
        {
        case GL_RED:
            return TEX_R8;
        case GL_RG:
            return TEX_RG8;
        case GL_RGB:
            return TEX_RGB8;
        default:
        case GL_RGBA:
            return TEX_RGBA8;
        }
    }
}

GL_APICALL void GL_APIENTRY glReadnPixels (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, void *data) 
{
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) SET_ERROR_AND_RETURN(GL_INVALID_FRAMEBUFFER_OPERATION);

    if (!_framebuffer_binding) NOT_IMPLEMENTED; // TODO: Context related operation

    gl_framebuffer_t *framebuffer = gl_get_binded_framebuffer();
    
    if (framebuffer->color_attachment0.binding == 0) SET_ERROR_AND_RETURN(GL_INVALID_OPERATION);

    uint32_t mode = gl_get_texture_mode_from_format_type(format, type);
    
    orch_readnpixels(orch, framebuffer->id, x, y, width, height, mode, 0, 0, data);
}

GL_APICALL void GL_APIENTRY glEGLReadnPixels (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, GLboolean swap_rb, GLboolean swap_y, void *data) 
{
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) SET_ERROR_AND_RETURN(GL_INVALID_FRAMEBUFFER_OPERATION);

    if (!_framebuffer_binding) NOT_IMPLEMENTED; // TODO: Context related operation

    gl_framebuffer_t *framebuffer = gl_get_binded_framebuffer();
    
    if (framebuffer->color_attachment0.binding == 0) SET_ERROR_AND_RETURN(GL_INVALID_OPERATION);

    uint32_t mode = gl_get_texture_mode_from_format_type(format, type);
    
    orch_readnpixels(orch, framebuffer->id, x, y, width, height, mode, swap_rb, swap_y, data);
}

GL_APICALL void GL_APIENTRY glRenderbufferStorage (GLenum target, GLenum internalformat, GLsizei width, GLsizei height) 
{
    if (target != GL_RENDERBUFFER) SET_ERROR_AND_RETURN(GL_INVALID_ENUM);
    
    if (is_valid_renderbuffer_internalformat(internalformat) == GL_FALSE) SET_ERROR_AND_RETURN(GL_INVALID_ENUM);

    if (width > CR_MAXVIEWPORT_SIZE || height > CR_MAXVIEWPORT_SIZE) SET_ERROR_AND_RETURN(GL_INVALID_VALUE);
    
    if (width == 0 || height == 0) NOT_IMPLEMENTED;

    if (_renderbuffer_binding == 0) SET_ERROR_AND_RETURN(GL_INVALID_OPERATION);

    gl_renderbuffer_t *renderbuffer = &_renderbuffers[_renderbuffer_binding-1];
    
    if (renderbuffer->width != 0 || renderbuffer->height != 0) SET_ERROR_AND_RETURN(GL_INVALID_OPERATION); 

    uint32_t mode = get_texture_mode_from_internalformat(internalformat);

    renderbuffer->id = orch_create_renderbuffer(orch, width, height, mode);
    renderbuffer->width = width;
    renderbuffer->height = height;
}

GL_APICALL void GL_APIENTRY glSampleCoverage (GLfloat value, GLboolean invert) 
{
    NOT_IMPLEMENTED; // TODO for multisampling
}

GL_APICALL void GL_APIENTRY glScissor (GLint x, GLint y, GLsizei width, GLsizei height) 
{
    if (width < 0 || height < 0) SET_ERROR_AND_RETURN(GL_INVALID_VALUE);
    
    _scissor_data = (scissor_data_t) {
        .left = x,
        .bottom = y,
        .width = width,
        .height = height,
    };
}

GL_APICALL void GL_APIENTRY glStencilFunc (GLenum func, GLint ref, GLuint mask) 
{
    glStencilFuncSeparate (GL_FRONT_AND_BACK, func, ref, mask);
}

GL_APICALL void GL_APIENTRY glStencilFuncSeparate (GLenum face, GLenum func, GLint ref, GLuint mask) 
{
    if (is_valid_face(face) == GL_FALSE) SET_ERROR_AND_RETURN(GL_INVALID_ENUM);

    switch (func)
    {
        default: 
            SET_ERROR_AND_RETURN(GL_INVALID_ENUM);
        case GL_NEVER:
        case GL_LESS:
        case GL_EQUAL:
        case GL_LEQUAL:
        case GL_GREATER:
        case GL_NOTEQUAL:
        case GL_GEQUAL:
        case GL_ALWAYS:
            ;
    }

    switch (face)
    {
        case GL_FRONT:
        case GL_FRONT_AND_BACK:
            set_stencil_function(&_stencil_data.front.function, func, ref, mask);
        default:
            ;
    }

    switch (face)
    {
        case GL_BACK:
        case GL_FRONT_AND_BACK:
            set_stencil_function(&_stencil_data.back.function, func, ref, mask);
        default:
            ;
    }

    rop_config_updated = 1;
}

GL_APICALL void GL_APIENTRY glStencilMask (GLuint mask) 
{
    glStencilMaskSeparate(GL_FRONT_AND_BACK, mask);
}

GL_APICALL void GL_APIENTRY glStencilMaskSeparate (GLenum face, GLuint mask) 
{
    if (is_valid_face(face) == GL_FALSE) SET_ERROR_AND_RETURN(GL_INVALID_ENUM);

    switch (face)
    {
        case GL_FRONT:
        case GL_FRONT_AND_BACK:
            _masks.stencil.front = mask;
        default:
            ;
    }

    switch (face)
    {
        case GL_BACK:
        case GL_FRONT_AND_BACK:
            _masks.stencil.back = mask;
        default:
            ;
    }

    rop_config_updated = 1;
}



GL_APICALL void GL_APIENTRY glStencilOp (GLenum fail, GLenum zfail, GLenum zpass)
{
    glStencilOpSeparate(GL_FRONT_AND_BACK, fail, zfail, zpass);
}


GL_APICALL void GL_APIENTRY glStencilOpSeparate (GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass) 
{
    if (is_valid_face(face) == GL_FALSE) SET_ERROR_AND_RETURN(GL_INVALID_ENUM);

    if (is_valid_stencil_operation(sfail) == GL_FALSE) SET_ERROR_AND_RETURN(GL_INVALID_ENUM);
    
    if (is_valid_stencil_operation(dpfail) == GL_FALSE) SET_ERROR_AND_RETURN(GL_INVALID_ENUM);

    if (is_valid_stencil_operation(dppass) == GL_FALSE) SET_ERROR_AND_RETURN(GL_INVALID_ENUM);

    switch (face)
    {
        case GL_FRONT:
        case GL_FRONT_AND_BACK:
            set_stencil_operation(&_stencil_data.front.operation, sfail, dpfail, dppass);
        default:
        ;
    }

    switch (face)
    {
        case GL_BACK:
        case GL_FRONT_AND_BACK:
            set_stencil_operation(&_stencil_data.back.operation, sfail, dpfail, dppass);
        default:
        ;
    }

    rop_config_updated = 1;
}

GL_APICALL void GL_APIENTRY glTexStorage2D (GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height) 
{
	if (target != GL_TEXTURE_2D) SET_ERROR_AND_RETURN(GL_INVALID_ENUM);

    if (!_texture_binding) SET_ERROR_AND_RETURN(GL_INVALID_OPERATION);
    
    if (width < 1 || height < 1 || levels < 1) SET_ERROR_AND_RETURN(GL_INVALID_VALUE);
    
    if (levels > (int) log2f(MAX(width,height)) + 1) SET_ERROR_AND_RETURN(GL_INVALID_OPERATION);
    
    if (levels != 1 && (IS_POWER_OF_2(width) || IS_POWER_OF_2(height))) SET_ERROR_AND_RETURN(GL_INVALID_OPERATION);
    
    gl_texture_t* texture = &_textures[_texture_binding-1];

    if (texture->width || texture->height) SET_ERROR_AND_RETURN(GL_INVALID_OPERATION); 
    
    if (levels != 1) NOT_IMPLEMENTED; // Mipmaps not supported yet

    uint32_t texture_mode = get_texture_mode_from_internalformat(internalformat);
    texture->id = orch_create_2d_texture(orch, width, height, texture_mode);
    texture->width = width;
    texture->height = height;
    texture->internalformat = internalformat;
    texture->levels = levels;
}

GL_APICALL void GL_APIENTRY glTexParameterf (GLenum target, GLenum pname, GLfloat param) 
{
    SET_ERROR_AND_RETURN(GL_INVALID_ENUM);
}

GL_APICALL void GL_APIENTRY glTexParameterfv (GLenum target, GLenum pname, const GLfloat *params)
{
    SET_ERROR_AND_RETURN(GL_INVALID_ENUM);
}

GL_APICALL void GL_APIENTRY glTexParameteri (GLenum target, GLenum pname, GLint param) 
{
    if (target != GL_TEXTURE_2D) SET_ERROR_AND_RETURN(GL_INVALID_ENUM);

    if (is_valid_tex_parameter(pname, param) == GL_FALSE) SET_ERROR_AND_RETURN(GL_INVALID_ENUM); 

    if (!_texture_binding) SET_ERROR_AND_RETURN(GL_INVALID_OPERATION);
    
    GLint* tex_parameter = get_tex_parameter_pointer(pname);

    *tex_parameter = param;
}

GL_APICALL void GL_APIENTRY glTexParameteriv (GLenum target, GLenum pname, const GLint *params) 
{
    if (target != GL_TEXTURE_2D) SET_ERROR_AND_RETURN(GL_INVALID_ENUM);

    if (is_valid_tex_parameter(pname, *params) == GL_FALSE) SET_ERROR_AND_RETURN(GL_INVALID_ENUM); 

    if (!_texture_binding) SET_ERROR_AND_RETURN(GL_INVALID_OPERATION);
    
    GLint* tex_parameter = get_tex_parameter_pointer(pname);

    *tex_parameter = *params;
}

GL_APICALL void GL_APIENTRY glTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels) 
{
    if (target != GL_TEXTURE_2D) SET_ERROR_AND_RETURN(GL_INVALID_ENUM);
    
    if (!_texture_binding) SET_ERROR_AND_RETURN(GL_INVALID_OPERATION);

    gl_texture_t *texture = &_textures[_texture_binding-1];
    
    if (level < 0 || level > (int) log2f(MAX(texture->width,texture->height)))
        SET_ERROR_AND_RETURN(GL_INVALID_VALUE);

    if (xoffset < 0 || yoffset < 0 || xoffset + width > texture->width || yoffset + height > texture->height)
        SET_ERROR_AND_RETURN(GL_INVALID_VALUE);

    if (level != 0) NOT_IMPLEMENTED; // Mipmaps not supported yet

    if (xoffset != 0 || yoffset != 0) NOT_IMPLEMENTED; // Subtextures not supported yet
    
    if (! (texture->internalformat == GL_RGBA8 && format == GL_RGBA && type == GL_UNSIGNED_BYTE) ) NOT_IMPLEMENTED; // Only RGBA8 supported yet

    uint32_t texture_mode = gl_get_texture_mode_from_format_type(format, type);

    orch_write_2d_texture(orch, texture->id, xoffset, yoffset, width, height, texture_mode, pixels);
}

#define GENERIC_UNIFORM(_SIZE, _GL_TYPE, _LOCATION, _TYPE, ...) \
{ \
    if (is_valid_uniform_data(_LOCATION, _SIZE, _GL_TYPE) == GL_FALSE) SET_ERROR_AND_RETURN(GL_INVALID_OPERATION); \
    _TYPE array[] = {__VA_ARGS__}; \
    set_uniform_data(location, sizeof(array), array); \
}

#define GENERIC_UNIFORM_V(_SIZE, _TYPE, _LOCATION, _ARRAY) \
{ \
    if (is_valid_uniform_data(_LOCATION, _SIZE, _TYPE) == GL_FALSE) SET_ERROR_AND_RETURN(GL_INVALID_OPERATION); \
    set_uniform_data(location, sizeof(_ARRAY[0])*_SIZE, _ARRAY); \
}

GL_APICALL void GL_APIENTRY glUniform1f (GLint location, GLfloat v0)
{
    GENERIC_UNIFORM(1, GL_FLOAT, location, GLfloat, v0);
}

GL_APICALL void GL_APIENTRY glUniform1fv (GLint location, GLsizei count, const GLfloat *value) 
{
    GENERIC_UNIFORM_V(1, GL_FLOAT, location, value);
}

GL_APICALL void GL_APIENTRY glUniform1i (GLint location, GLint v0)
{
    GENERIC_UNIFORM(1, GL_INT, location, GLint, v0);
}

GL_APICALL void GL_APIENTRY glUniform1iv (GLint location, GLsizei count, const GLint *value) 
{
    GENERIC_UNIFORM_V(1, GL_INT, location, value);
}

GL_APICALL void GL_APIENTRY glUniform2f (GLint location, GLfloat v0, GLfloat v1)
{
    GENERIC_UNIFORM(2, GL_FLOAT, location, GLfloat, v0, v1);
}

GL_APICALL void GL_APIENTRY glUniform2fv (GLint location, GLsizei count, const GLfloat *value)
{
    GENERIC_UNIFORM_V(2, GL_FLOAT, location, value);
}

GL_APICALL void GL_APIENTRY glUniform2i (GLint location, GLint v0, GLint v1)
{
    GENERIC_UNIFORM(2, GL_INT, location, GLint, v0, v1);
}

GL_APICALL void GL_APIENTRY glUniform2iv (GLint location, GLsizei count, const GLint *value)
{
    GENERIC_UNIFORM_V(2, GL_INT, location, value);
}

GL_APICALL void GL_APIENTRY glUniform3f (GLint location, GLfloat v0, GLfloat v1, GLfloat v2)
{
    GENERIC_UNIFORM(3, GL_FLOAT, location, GLfloat, v0, v1, v2);
}

GL_APICALL void GL_APIENTRY glUniform3fv (GLint location, GLsizei count, const GLfloat *value)
{
    GENERIC_UNIFORM_V(3, GL_FLOAT, location, value);
}

GL_APICALL void GL_APIENTRY glUniform3i (GLint location, GLint v0, GLint v1, GLint v2)
{
    GENERIC_UNIFORM(3, GL_INT, location, GLint, v0, v1, v2);
}

GL_APICALL void GL_APIENTRY glUniform3iv (GLint location, GLsizei count, const GLint *value)
{
    GENERIC_UNIFORM_V(3, GL_INT, location, value);
}

GL_APICALL void GL_APIENTRY glUniform4f (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)
{
    GENERIC_UNIFORM(4, GL_FLOAT, location, GLfloat, v0, v1, v2, v3);
}

GL_APICALL void GL_APIENTRY glUniform4fv (GLint location, GLsizei count, const GLfloat *value) 
{
    GENERIC_UNIFORM_V(4, GL_FLOAT, location, value);
}

GL_APICALL void GL_APIENTRY glUniform4i (GLint location, GLint v0, GLint v1, GLint v2, GLint v3)
{
    GENERIC_UNIFORM(4, GL_INT, location, GLint, v0, v1, v2, v3);
}

GL_APICALL void GL_APIENTRY glUniform4iv (GLint location, GLsizei count, const GLint *value)
{
    GENERIC_UNIFORM_V(4, GL_INT, location, value);
}

GL_APICALL void GL_APIENTRY glUniformMatrix2fv (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    if (transpose != GL_FALSE) SET_ERROR_AND_RETURN(GL_INVALID_VALUE);

    GENERIC_UNIFORM_V(8, GL_FLOAT, location, value);
}

GL_APICALL void GL_APIENTRY glUniformMatrix3fv (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    if (transpose != GL_FALSE) SET_ERROR_AND_RETURN(GL_INVALID_VALUE);

    GENERIC_UNIFORM_V(16, GL_FLOAT, location, value);
}
GL_APICALL void GL_APIENTRY glUniformMatrix4fv (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
    if (transpose != GL_FALSE) SET_ERROR_AND_RETURN(GL_INVALID_VALUE);

    GENERIC_UNIFORM_V(16, GL_FLOAT, location, value);
}

#undef GENERIC_UNIFORM
#undef GENERIC_UNIFORM_V

GL_APICALL void GL_APIENTRY glUseProgram (GLuint program)
{
    _current_program = program;
}

GL_APICALL void GL_APIENTRY glVertexAttrib1f (GLuint index, GLfloat x) 
{
    gl_set_vertex_attribute(index, x, 0.f, 0.f, 1.f);
}

GL_APICALL void GL_APIENTRY glVertexAttrib1fv (GLuint index, const GLfloat *v) 
{
    gl_set_vertex_attribute(index, v[0], 0.f, 0.f, 1.f);
}

GL_APICALL void GL_APIENTRY glVertexAttrib2f (GLuint index, GLfloat x, GLfloat y) 
{
    gl_set_vertex_attribute(index, x, y, 0.f, 1.f);
}

GL_APICALL void GL_APIENTRY glVertexAttrib2fv (GLuint index, const GLfloat *v) 
{
    gl_set_vertex_attribute(index, v[0], v[1], 0.f, 1.f);
}

GL_APICALL void GL_APIENTRY glVertexAttrib3f (GLuint index, GLfloat x, GLfloat y, GLfloat z) 
{
    gl_set_vertex_attribute(index, x, y, z, 1.f);
}

GL_APICALL void GL_APIENTRY glVertexAttrib3fv (GLuint index, const GLfloat *v) 
{
    gl_set_vertex_attribute(index, v[0], v[1], v[2], 1.f);
}

GL_APICALL void GL_APIENTRY glVertexAttrib4f (GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) 
{
    gl_set_vertex_attribute(index, x, y, z, w);
}

GL_APICALL void GL_APIENTRY glVertexAttrib4fv (GLuint index, const GLfloat *v) 
{
    gl_set_vertex_attribute(index, v[0], v[1], v[2], v[3]);
}

GL_APICALL void GL_APIENTRY glVertexAttribPointer (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer) 
{
    unsigned int va_size, va_type, va_normalize, va_active_pointer;

    if (index >= DEVICE_VERTEX_ATTRIBUTE_SIZE) SET_ERROR_AND_RETURN(GL_INVALID_VALUE);

    if (gl_size_to_vertex_attribute_size(size, &va_size) == GL_FALSE) SET_ERROR_AND_RETURN(GL_INVALID_VALUE);

    if (gl_type_to_vertex_attribute_type(type, &va_type) == GL_FALSE) SET_ERROR_AND_RETURN(GL_INVALID_ENUM);

    if (normalized == GL_FALSE) 
    {
        va_normalize = 0;
    }
    else 
    {
        va_normalize = VERTEX_ATTRIBUTE_NORMALIZE;
    }
    
    if (stride < 0) SET_ERROR_AND_RETURN(GL_INVALID_VALUE);
    
    if (stride == 0) 
    {
        stride = sizeof_vertex_attribute_type(type)*size;
    }

    va_active_pointer = vertex_attribute_datas[index].misc & VERTEX_ATTRIBUTE_ACTIVE_POINTER;

    uint32_t is_buffer_bound = _buffer_binding;

    vertex_attibute_updated = 1;
    set_vertex_attribute(
        &vertex_attribute_datas[index], 
        is_buffer_bound ? (uintptr_t) pointer : 0, 
        stride, 
        va_type, 
        va_size, 
        va_normalize, 
        va_active_pointer
    );

    vertex_attribute_binding[index] = (vertex_attribute_binding_t) {
        .binding = _buffer_binding,
        .pointer = pointer,
    };
}

GL_APICALL void GL_APIENTRY glViewport (GLint x, GLint y, GLsizei width, GLsizei height)
{
    _viewport.x=x;
    _viewport.y=y;
    _viewport.width=width;
    _viewport.height=height;
}

static GLboolean is_valid_blend_equation(GLenum mode)
{
    switch (mode)
    {
        default:
            return GL_FALSE;
        case GL_FUNC_ADD:
        case GL_FUNC_SUBTRACT:
        case GL_FUNC_REVERSE_SUBTRACT:
            return GL_TRUE;
    }
}

static GLboolean is_valid_blend_function(GLenum mode)
{
    switch (mode)
    {
        default:
            return GL_FALSE;
        case GL_ZERO:
        case GL_ONE:
        case GL_SRC_COLOR:
        case GL_ONE_MINUS_SRC_COLOR:
        case GL_SRC_ALPHA:
        case GL_ONE_MINUS_SRC_ALPHA:
        case GL_DST_ALPHA:
        case GL_ONE_MINUS_DST_ALPHA:
        case GL_DST_COLOR:
        case GL_ONE_MINUS_DST_COLOR:
        case GL_SRC_ALPHA_SATURATE:             
        case GL_CONSTANT_COLOR:       
        case GL_ONE_MINUS_CONSTANT_COLOR:       
        case GL_CONSTANT_ALPHA:       
        case GL_ONE_MINUS_CONSTANT_ALPHA:
            return GL_TRUE;
    }
}

static GLboolean is_valid_program(GLuint program)
{
    return program <= _program_sz && program != 0;
}

static GLboolean is_valid_renderbuffer_internalformat(GLenum internalformat) 
{
    switch (internalformat)
    {
        case GL_DEPTH_COMPONENT16:
        case GL_RGBA4:
        case GL_RGB5_A1:
        case GL_RGB565:
        case GL_STENCIL_INDEX8:
            return 1;
        default:
            return 0;
    }
}

static attachment_size_t get_attachment_size(attachment_t* attachment) 
{
    attachment_size_t size;

    if (attachment->target == GL_RENDERBUFFER) 
    {
        gl_renderbuffer_t *renderbuffer = &_renderbuffers[attachment->binding-1];
        size = (attachment_size_t) {
            .width  = renderbuffer->width,
            .height = renderbuffer->height,
        };
    } 
    else // GL_TEXTURE_2D 
    {
        gl_texture_t *texture = &_textures[attachment->binding-1];
        size = (attachment_size_t) {
            .width  = texture->width,
            .height = texture->height,
        };
    }

    return size;
}

static uint32_t get_clear_color(GLfloat r, GLfloat g, GLfloat b, GLfloat a) 
{
    return rgba_to_uint32(r, g, b, a);
}

static uint16_t get_clear_depth(GLfloat depth) 
{
    return depth*0xFFFFu;
}

static uint8_t get_clear_stencil(GLuint stencil) 
{
    return stencil;
}

static cl_ulong get_clear_write_values()
{
    cl_ulong clear_write_values;

    cl_ulong clear_color = get_clear_color(
        _clear_data.color.red, 
        _clear_data.color.green, 
        _clear_data.color.blue, 
        _clear_data.color.alpha
    );
    cl_ulong clear_depth = get_clear_depth(_clear_data.depth);
    cl_ulong clear_stencil = get_clear_stencil(_clear_data.stencil);

    clear_write_values = 
        clear_color      <<  0 |
        clear_depth      << 32 |
        clear_stencil    << 48 ;

    return clear_write_values;
};

static enabled_data_t get_clear_enabled_data(GLbitfield mask) 
{
    enabled_data_t enabled_data;

    GLbitfield color_mask    = mask & GL_COLOR_BUFFER_BIT;
    GLbitfield depth_mask    = mask & GL_DEPTH_BUFFER_BIT;
    GLbitfield stencil_mask  = mask & GL_STENCIL_BUFFER_BIT;

    set_enabled_color_data(
        &enabled_data,
        color_mask ? _masks.color.r : 0,
        color_mask ? _masks.color.g : 0,
        color_mask ? _masks.color.b : 0,
        color_mask ? _masks.color.a : 0
    );

    set_enabled_depth_data(
        &enabled_data,
        depth_mask ? _masks.depth : 0
    );
    
    set_enabled_stencil_data(
        &enabled_data,
        stencil_mask ? _masks.stencil.front : 0
    );

    return enabled_data;
};

static depth_data_t get_depth_data_from_gl_state()
{
    return get_depth_data(
        gl_depth_func_to_depth_func(_depth_func),
        (uint16_t) (_depth_range.n * 0xFFFFu),
        (uint16_t) (_depth_range.f * 0xFFFFu)
    );
}

static GLboolean* get_enabled_pointer(GLenum cap)
{
    switch (cap)
    {
    case GL_SCISSOR_TEST:
        return &_enableds.scissor_test;
    case GL_STENCIL_TEST:
        return &_enableds.stencil_test;
    case GL_DEPTH_TEST:
        return &_enableds.depth_test;
    case GL_BLEND:
        return &_enableds.blend;
    case GL_DITHER:
        return &_enableds.dither;
    case GL_CULL_FACE:
        return &_enableds.cull_face;
    default:
        return NULL;
    }
}



static render_mode_t get_render_mode(GLenum mode)
{
    render_mode_t render_mode_flags;

    render_mode_flags.flags = RENDER_MODE_FLAG_ENABLE_LERP; // always lerp enabled

    if (_enableds.depth_test) render_mode_flags.flags |= RENDER_MODE_FLAG_ENABLE_DEPTH;

    if (_enableds.stencil_test) render_mode_flags.flags |= RENDER_MODE_FLAG_ENABLE_STENCIL;

    if (_enableds.blend) render_mode_flags.flags |= RENDER_MODE_FLAG_ENABLE_BLENDER;

    if (_enableds.cull_face) 
    {
        switch (_rasterization_data.cull_face)
        {
            case GL_FRONT:
            case GL_FRONT_AND_BACK:
                render_mode_flags.flags |= RENDER_MODE_FLAG_ENABLE_CULL_FRONT;
            default:
                ;
        }

        switch (_rasterization_data.cull_face)
        {
            case GL_BACK:
            case GL_FRONT_AND_BACK:
                render_mode_flags.flags |= RENDER_MODE_FLAG_ENABLE_CULL_BACK;
            default:
                ;
        }
    }
    
    switch (mode)
    {
        case GL_TRIANGLE_FAN:
            render_mode_flags.flags |= RENDER_MODE_FLAG_TRIANGLE_FAN;
            break;
        case GL_TRIANGLE_STRIP:
            render_mode_flags.flags |= RENDER_MODE_FLAG_TRIANGLE_STRIP;
            break;
        default:
            ;
    }

    return render_mode_flags;
}

static uint32_t get_texture_mode_from_internalformat(GLenum internalformat)
{
    switch (internalformat) {
        default:
        case GL_RGBA8:
            return TEX_RGBA8;
        case GL_RGB8:
            return TEX_RGB8;
        case GL_RG8:
            return TEX_RG8;
        case GL_RGBA4:
            return TEX_RGBA4;
        case GL_RGB5_A1:
            return TEX_RGB5_A1;
        case GL_RGB565:
            return TEX_RGB565;
        case GL_R8:
            return TEX_R8;
    }
}

static void gl_set_vertex_attribute(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    if (index >= DEVICE_VERTEX_ATTRIBUTE_SIZE) SET_ERROR_AND_RETURN(GL_INVALID_VALUE);

    vertex_attibute_updated = 1;
    vertex_attributes[index][0] = x;
    vertex_attributes[index][1] = y;
    vertex_attributes[index][2] = z;
    vertex_attributes[index][3] = w;
}

static size_t sizeof_gl_type(GLenum type)
{
    switch (type)
    {
        default:
        case GL_BYTE:
        case GL_UNSIGNED_BYTE:
            return 1;
        case GL_SHORT:
        case GL_UNSIGNED_SHORT:
            return 2;
        case GL_INT:
        case GL_UNSIGNED_INT:
        case GL_FLOAT:
            return 4;
    }
}

static size_t sizeof_vertex_attribute_type(GLenum type)
{
    switch (type)
    {
        case GL_BYTE:
        case GL_UNSIGNED_BYTE:
            return 1;
        case GL_SHORT:
        case GL_UNSIGNED_SHORT:
            return 2;
        case GL_INT:
        case GL_UNSIGNED_INT:
        case GL_FLOAT:
            return 4;
        default:
            return 0;
    }
}

static void set_stencil_operation(stencil_operation_t *stencil_operation, GLenum sfail, GLenum dfail, GLenum dpass)
{
    *stencil_operation = (stencil_operation_t) 
    {
        .sfail = sfail,
        .dpfail = dfail,
        .dpass = dpass
    };
}

static GLboolean is_valid_stencil_operation(GLenum operation)
{
    switch (operation)
    {
        default:
            return GL_FALSE;
        case GL_KEEP:
        case GL_ZERO:
        case GL_REPLACE:
        case GL_INCR:
        case GL_DECR:
        case GL_INVERT:
        case GL_INCR_WRAP:
        case GL_DECR_WRAP:
            ;
    }

    return GL_TRUE;
}

static uint32_t rgba_to_uint32(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    return 
        ((uint32_t)(r * 0xFF) <<  0) |
        ((uint32_t)(g * 0xFF) <<  8) |
        ((uint32_t)(b * 0xFF) << 16) |
        ((uint32_t)(a * 0xFF) << 24) ;
}

static uint32_t gl_tex_wrap_to_tex_wrap(GLint param)
{
    switch (param)
    {
        default:
        case GL_REPEAT: 
            return TEXTURE_WRAP_REPEAT;
        case GL_CLAMP_TO_EDGE: 
            return TEXTURE_WRAP_CLAMP_TO_EDGE;
        case GL_MIRRORED_REPEAT: 
            return TEXTURE_WRAP_MIRRORED_REPEAT;
    }
}

static uint32_t gl_tex_filter_to_tex_filter(GLint param)
{
    switch (param)
    {
        default:
        case GL_NEAREST:
            return TEXTURE_FILTER_NEAREST;
        case GL_LINEAR:
            return TEXTURE_FILTER_LINEAR;
        case GL_NEAREST_MIPMAP_NEAREST:
            return TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST;
        case GL_NEAREST_MIPMAP_LINEAR:
            return TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR;
        case GL_LINEAR_MIPMAP_NEAREST:
            return TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST;
        case GL_LINEAR_MIPMAP_LINEAR:
            return TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR;
    }
}

static GLboolean is_valid_tex_parameter(GLenum pname, GLint param)
{
    switch (pname)
    {
        case GL_TEXTURE_WRAP_S:
        case GL_TEXTURE_WRAP_T:
            switch (param)
            {
                default: 
                    return GL_FALSE;
                case GL_CLAMP_TO_EDGE:
                case GL_REPEAT:
                case GL_MIRRORED_REPEAT:
                    ;
            }
            break;
        case GL_TEXTURE_MAG_FILTER:
            switch (param)
            {
                default: 
                    return GL_FALSE;
                case GL_NEAREST:
                case GL_LINEAR:
                    ;
            }
            break;
        case GL_TEXTURE_MIN_FILTER:
            switch (param)
            {
                default: 
                    return GL_FALSE;
                case GL_NEAREST:
                case GL_LINEAR:
                case GL_NEAREST_MIPMAP_NEAREST:
                case GL_NEAREST_MIPMAP_LINEAR:
                case GL_LINEAR_MIPMAP_NEAREST:
                case GL_LINEAR_MIPMAP_LINEAR:
                    ;
            }
            break;
        default: 
            return GL_FALSE;
    }

    return GL_TRUE;
}

static GLint* get_tex_parameter_pointer(GLenum pname) 
{
    gl_texture_t *texture = &_textures[_texture_binding-1];

    switch (pname)
    {
        case GL_TEXTURE_WRAP_S:
            return &texture->wraps.s;        
        case GL_TEXTURE_WRAP_T:
            return &texture->wraps.t; 
        case GL_TEXTURE_MIN_FILTER:
            return &texture->wraps.min_filter; 
        case GL_TEXTURE_MAG_FILTER:
            return &texture->wraps.mag_filter; 
        default:
            return NULL;
    }
}

static GLboolean is_valid_draw_mode(GLenum mode)
{
    switch (mode)
    {
        default:
            return GL_FALSE;
        case GL_POINTS:
        case GL_LINES:
        case GL_LINE_LOOP:
        case GL_LINE_STRIP:
        case GL_TRIANGLES:
        case GL_TRIANGLE_STRIP:
        case GL_TRIANGLE_FAN:
            return GL_TRUE;
    }
}

static uint32_t get_deferred_clear()
{
    return c_clear_enabled_data != 0;
}

static uint32_t get_blending_color() 
{
    return rgba_to_uint32(
        _blend_data.color.red, 
        _blend_data.color.green, 
        _blend_data.color.blue, 
        _blend_data.color.alpha
    );
}

static blending_data_t get_blending_data() 
{
    blending_data_t blending_data = {0};

    set_blending_data_equation(
        &blending_data, 
        gl_blend_equation_to_blend_equation(_blend_data.equation.modeRGB), 
        gl_blend_equation_to_blend_equation(_blend_data.equation.modeAlpha)
    );

    set_blending_data_function_src(
        &blending_data, 
        gl_blend_func_to_blend_func(_blend_data.func.srcRGB),
        gl_blend_func_to_blend_func(_blend_data.func.srcAlpha)
    );

    set_blending_data_function_dst(
        &blending_data, 
        gl_blend_func_to_blend_func(_blend_data.func.dstRGB),
        gl_blend_func_to_blend_func(_blend_data.func.dstAlpha)
    );

    return blending_data;
}

inline static enabled_data_t get_enabled_data_from_gl_masks(mask_container_t masks) 
{
    enabled_data_t enabled_data;

    set_enabled_color_data(
        &enabled_data,
        _masks.color.r,
        _masks.color.g,
        _masks.color.b,
        _masks.color.a
    );

    set_enabled_depth_data(
        &enabled_data,
        _masks.depth
    );
    
    set_enabled_stencil_data(
        &enabled_data,
        _masks.stencil.front
    );

    return enabled_data;
}

static gl_stencil_data_t get_stencil_data_from_gl_state() 
{
    gl_stencil_data_t stencil_data;

    
    set_stencil_data_func(
        &stencil_data,
        FRONT,
        gl_stencil_func_to_stencil_func(_stencil_data.front.function.func),
        _stencil_data.front.function.mask,
        _stencil_data.front.function.ref
    );

    set_stencil_data_func(
        &stencil_data,
        BACK,
        gl_stencil_func_to_stencil_func(_stencil_data.back.function.func),
        _stencil_data.back.function.mask,
        _stencil_data.back.function.ref
    );

    set_stencil_data_op(
        &stencil_data,
        FRONT,
        gl_stencil_op_to_stencil_op(_stencil_data.front.operation.sfail),
        gl_stencil_op_to_stencil_op(_stencil_data.front.operation.dpass),
        gl_stencil_op_to_stencil_op(_stencil_data.front.operation.dpfail)
    );

    set_stencil_data_op(
        &stencil_data,
        BACK,
        gl_stencil_op_to_stencil_op(_stencil_data.back.operation.sfail),
        gl_stencil_op_to_stencil_op(_stencil_data.back.operation.dpass),
        gl_stencil_op_to_stencil_op(_stencil_data.back.operation.dpfail)
    );

    return stencil_data;
}

static rop_config_t get_rop_config(GLenum mode) 
{
    rop_config_t rop_config;
    
    rop_config = (rop_config_t) 
    {
        .render_mode    = get_render_mode(mode),
        .blending_data  = get_blending_data(),
        .blending_color = get_blending_color(),
        .stencil_data   = get_stencil_data_from_gl_state().front_misc, // TODO: add for both
        .depth_data     = get_depth_data_from_gl_state().misc,
        .enabled_data   = get_enabled_data_from_gl_masks(_masks),
    };
    
    return rop_config;
}

static void set_stencil_function(stencil_function_t *stencil_function, GLenum func, GLint ref, GLuint mask)
{
    *stencil_function = (stencil_function_t) 
    {
        .func = func,
        .ref = ref,
        .mask = mask,
    };
}

static GLboolean is_valid_face(GLenum face)
{
    switch (face)
    {
        default: 
            return GL_FALSE;
        case GL_FRONT:
        case GL_BACK:
        case GL_FRONT_AND_BACK:
            return GL_TRUE;
    }
}

static void set_uniform_data(GLint location, size_t size, const void* data)
{
    gl_program_t *program = gl_get_current_program();
    // printf("location=%d offset=%d, size=%ld, data*=%d\n",location, program->uniform_arg_datas[location].offset, size, *((uint32_t*)data));
    memcpy(program->uniform_data + program->uniform_arg_datas[location].offset, data, size);
    _uniform_data_updated = 1;
}

static GLboolean is_valid_arg_size_type(arg_data_t* arg_data, size_t size, GLenum type)
{
    return arg_data->type == type && arg_data->size == size ? GL_TRUE : GL_FALSE;
}

static GLboolean is_valid_uniform_data(GLint location, size_t size, GLenum type)
{
    if (_current_program == 0) return GL_FALSE;

    gl_program_t *program = gl_get_current_program();

    if (location >= program->uniform_sz) return GL_FALSE;

    arg_data_t *arg_data = &program->uniform_arg_datas[location];

    if (is_valid_arg_size_type(arg_data, size, type) == GL_FALSE) return GL_FALSE;

    return GL_TRUE;
}

static GLboolean gl_type_to_vertex_attribute_type(GLenum gl_type, unsigned int* va_type) 
{
    #define CASE_GL_TO_VERTEX_ATTRIBUTE_TYPE(type) \
        case GL_##type: *va_type = VERTEX_ATTRIBUTE_TYPE_##type; break;

    switch (gl_type)
    {
        CASE_GL_TO_VERTEX_ATTRIBUTE_TYPE(BYTE);
        CASE_GL_TO_VERTEX_ATTRIBUTE_TYPE(UNSIGNED_BYTE);
        CASE_GL_TO_VERTEX_ATTRIBUTE_TYPE(SHORT);
        CASE_GL_TO_VERTEX_ATTRIBUTE_TYPE(UNSIGNED_SHORT);
        CASE_GL_TO_VERTEX_ATTRIBUTE_TYPE(FLOAT);
        default:
            return GL_FALSE;
    }

    #undef CASE_GL_TO_VERTEX_ATTRIBUTE_TYPE

    return GL_TRUE;
};

static GLboolean gl_size_to_vertex_attribute_size(GLint gl_size, unsigned int* va_size)
{
    #define CASE_GL_TO_VERTEX_ATTRIBUTE_SIZE(size) \
        case size: *va_size = VERTEX_ATTRIBUTE_SIZE_##size; break;

    switch (gl_size)
    {
        CASE_GL_TO_VERTEX_ATTRIBUTE_SIZE(1);
        CASE_GL_TO_VERTEX_ATTRIBUTE_SIZE(2);
        CASE_GL_TO_VERTEX_ATTRIBUTE_SIZE(3);
        CASE_GL_TO_VERTEX_ATTRIBUTE_SIZE(4);
        default:
            return GL_FALSE;
    }

    #undef CASE_GL_TO_VERTEX_ATTRIBUTE_SIZE

    return GL_TRUE;
}

static uint32_t gl_blend_equation_to_blend_equation(GLenum equation)
{
    switch (equation)
    {
        default:
        case GL_FUNC_ADD: 
            return BLEND_FUNC_ADD;
        case GL_FUNC_SUBTRACT:
            return BLEND_FUNC_SUBTRACT;
        case GL_FUNC_REVERSE_SUBTRACT:
            return BLEND_FUNC_REVERSE_SUBTRACT;
    }
}

static uint32_t gl_blend_func_to_blend_func(GLenum func)
{
    #define CASE(_NAME) \
        case GL_##_NAME: return BLEND_##_NAME;

    switch (func)
    {
        default:
        CASE(ZERO);
        CASE(ONE);
        CASE(ONE_MINUS_SRC_COLOR);
        CASE(SRC_ALPHA);
        CASE(ONE_MINUS_SRC_ALPHA);
        CASE(DST_ALPHA);
        CASE(ONE_MINUS_DST_ALPHA);
        CASE(DST_COLOR);
        CASE(ONE_MINUS_DST_COLOR);
        CASE(SRC_ALPHA_SATURATE);             
        CASE(CONSTANT_COLOR);       
        CASE(ONE_MINUS_CONSTANT_COLOR);       
        CASE(CONSTANT_ALPHA);       
        CASE(ONE_MINUS_CONSTANT_ALPHA);
    }

    #undef CASE
}

static uint32_t gl_stencil_func_to_stencil_func(GLenum func)
{
    #define CASE(_NAME) \
        case GL_##_NAME: return STENCIL_FUNC_##_NAME;

    switch (func)
    {
        default:
        CASE(NEVER);
        CASE(LESS);
        CASE(EQUAL);
        CASE(LEQUAL);
        CASE(GREATER);
        CASE(NOTEQUAL);
        CASE(GEQUAL);
        CASE(ALWAYS);
    }

    #undef CASE
}

static uint32_t gl_stencil_op_to_stencil_op(GLenum op)
{
    #define CASE(_NAME) \
        case GL_##_NAME: return STENCIL_OP_##_NAME;

    switch (op)
    {
        default:
        CASE(KEEP);
        CASE(ZERO);
        CASE(REPLACE);
        CASE(INCR);
        CASE(DECR);
        CASE(INVERT);
        CASE(INCR_WRAP);
        CASE(DECR_WRAP);
    }

    #undef CASE
}

static uint32_t gl_depth_func_to_depth_func(GLenum func)
{
    #define CASE(_NAME) \
        case GL_##_NAME: return DEPTH_FUNC_##_NAME;

    switch (func)
    {
        default:            
        CASE(NEVER)
        CASE(LESS)
        CASE(EQUAL)
        CASE(LEQUAL)
        CASE(GREATER)
        CASE(NOTEQUAL)
        CASE(GEQUAL)
        CASE(ALWAYS)
    }

    #undef CASE
}