#include <middleware/orchestrator.h>

#include <stdio.h>

#define NOT_IMPLEMENTED \
    { \
        printf("Funtion %s at %s:%d is not implemented.\n", __func__, __FILE__, __LINE__); \
        exit(1); \
    }

// TODO: circular dependency, maybe change behaviour 
static device_context_t* __orch_attach_new_context(orch_handler_t* orch, orch_framebuffer_handler_t* framebuffer, int move_fragment_data);


static orch_framebuffer_handler_t* __orch_get_framebuffer_from_id(orch_handler_t* orch, size_t framebuffer_id)
{
    return &orch->framebuffers[framebuffer_id];
}

static device_context_t* __orch_get_context_from_id(orch_handler_t* orch, size_t context_id)
{
    if (context_id >= DEVICE_CONTEXT_NUMBER) return NULL;

    return &orch->contexts[context_id];
}

static orch_framebuffer_handler_t* __orch_get_framebuffer_attached_to_context_id(orch_handler_t* orch, size_t context_id)
{
    return orch->context_framebuffer_attachments[context_id];
} 

static device_context_t* __orch_get_attached_context(orch_handler_t* orch, orch_framebuffer_handler_t* framebuffer)
{
    return __orch_get_context_from_id(orch,framebuffer->context_id);
}

static device_context_t* __orch_get_attached_or_attach_context(orch_handler_t* orch, orch_framebuffer_handler_t* framebuffer)
{
    if (framebuffer->context_id == -1) return __orch_attach_new_context(orch, framebuffer, 0);

    return __orch_get_attached_context(orch, framebuffer);
}

static uint32_t __orch_get_deferred_clear(orch_framebuffer_handler_t* framebuffer)
{
    return framebuffer->clear_state.enabled.misc ? 1 : 0;
}

static size_t __orch_get_num_triangles_from_vertices(render_mode_t mode, size_t num_vertices)
{
    if (is_render_mode_flag_triangle_fan(mode) || is_render_mode_flag_triangle_strip(mode))
        return (num_vertices >= 3) ? num_vertices - 2 : 0;

    return num_vertices / 3;
}

static size_t __orch_get_next_context_id(orch_handler_t* orch)
{
    size_t ret_value = orch->next_context_id;

    orch->next_context_id = (orch->next_context_id + 1) % DEVICE_CONTEXT_NUMBER;

    return ret_value; 
}

static uint8_t __orch_require_flush_vertices(orch_framebuffer_handler_t* framebuffer, render_mode_t mode, uint32_t config_id)
{
    if (is_render_mode_flag_triangle_fan(mode) || is_render_mode_flag_triangle_strip(mode)) return 1;

    if (framebuffer->draw_state.last_render_mode.flags != mode.flags) return 1;

    if (framebuffer->draw_state.last_config_id != config_id) return 1;

    return 1;
    // return 0; TODO: check logic to optimize and do not always flush
}

typedef enum {
    NONE = 0,
    SHADER_UPDATE,
    VERTEX_BUFFER_CAPACITY,
    TRIANGLE_BUFFER_CAPACITY
} __orch_flush_context_reason_t;

static __orch_flush_context_reason_t __orch_require_flush_context(
    orch_framebuffer_handler_t* framebuffer, 
    size_t shader_id, 
    render_mode_t mode, 
    size_t num_vertices
) {
    // no drawing done
    if (framebuffer->draw_state.assembled_triangles == 0 && framebuffer->draw_state.assembled_vertices == 0) return NONE;

    if (framebuffer->draw_state.shader_id != shader_id) return SHADER_UPDATE;

    // TODO: depends also on the varying size
    if (framebuffer->draw_state.assembled_vertices + num_vertices > DEVICE_VERTICES_SIZE) return VERTEX_BUFFER_CAPACITY;

    size_t pending_triangles = __orch_get_num_triangles_from_vertices(
        framebuffer->draw_state.last_render_mode, 
        framebuffer->draw_state.pending_vertices
    );

    size_t requested_triangles = __orch_get_num_triangles_from_vertices(mode, num_vertices);

    if (framebuffer->draw_state.assembled_triangles + pending_triangles + requested_triangles > DEVICE_MAX_NUMBER_TRIANGLES) return TRIANGLE_BUFFER_CAPACITY;

    // TODO: maybe take account segments available

    return NONE;
}

// Attach functions

static void __orch_attach_rw_image2d_colorbuffer(orch_handler_t* orch, size_t framebuffer_id, size_t color_id)
{
    orch_rw_image2d_handler_t* i_handler = &orch->rw_image2ds[color_id];
    orch_framebuffer_handler_t* f_handler = &orch->framebuffers[framebuffer_id];

    f_handler->colorbuffer_id = i_handler->image_id;
    f_handler->width = i_handler->width;
    f_handler->height = i_handler->height;
    f_handler->colorbuffer_mode = i_handler->mode;
}

static void __orch_attach_rw_image2d_depthbuffer(orch_handler_t* orch, size_t framebuffer_id, size_t depth_id)
{
    orch_rw_image2d_handler_t* i_handler = &orch->rw_image2ds[depth_id];
    orch_framebuffer_handler_t* f_handler = &orch->framebuffers[framebuffer_id];

    f_handler->depthbuffer_id = i_handler->image_id;
    f_handler->width = i_handler->width;
    f_handler->height = i_handler->height;
}

static void __orch_attach_rw_image2d_stencilbuffer(orch_handler_t* orch, size_t framebuffer_id, size_t stencil_id)
{
    orch_rw_image2d_handler_t* i_handler = &orch->rw_image2ds[stencil_id];
    orch_framebuffer_handler_t* f_handler = &orch->framebuffers[framebuffer_id];

    f_handler->stencilbuffer_id = i_handler->image_id;
    f_handler->width = i_handler->width;
    f_handler->height = i_handler->height;
}

// Work functions

static void __orch_flush_vertices(orch_handler_t* orch, orch_framebuffer_handler_t* framebuffer)
{
    if (framebuffer->draw_state.pending_vertices == 0) return;

    size_t pending_triangles = __orch_get_num_triangles_from_vertices(
        framebuffer->draw_state.last_render_mode, 
        framebuffer->draw_state.pending_vertices
    );

    size_t vertices_offset = framebuffer->draw_state.assembled_vertices - framebuffer->draw_state.pending_vertices;

    device_context_t* context = __orch_get_attached_context(orch, framebuffer);

    device_launch_arrays_triangle_assembly(
        context,
        framebuffer->draw_state.shader_id,
        framebuffer->draw_state.last_render_mode,
        framebuffer->draw_state.last_config_id,
        vertices_offset,
        framebuffer->draw_state.assembled_triangles,
        pending_triangles,
        framebuffer->width,
        framebuffer->height
    );

    framebuffer->draw_state.pending_vertices = 0;
    framebuffer->draw_state.assembled_triangles += pending_triangles;
}

static void __orch_flush_draw_state(orch_handler_t* orch, orch_framebuffer_handler_t* framebuffer)
{
    __orch_flush_vertices(orch, framebuffer);

    if (framebuffer->draw_state.assembled_triangles == 0) return;
    
    // printf("flushed state: assembled_triangles=%d\n", framebuffer->draw_state.assembled_triangles);
    uint32_t deferred_clear = __orch_get_deferred_clear(framebuffer);

    device_context_t* context = __orch_get_attached_context(orch, framebuffer);

    device_launch_bin_dispatch(context, framebuffer->draw_state.assembled_triangles, framebuffer->width, framebuffer->height);

    device_launch_tile_dispatch(context, deferred_clear, framebuffer->width, framebuffer->height);

    device_launch_fragment_shader(
        context,
        framebuffer->draw_state.shader_id,
        framebuffer->clear_state.data, 
        framebuffer->clear_state.enabled,
        framebuffer->colorbuffer_id,
        framebuffer->depthbuffer_id,
        framebuffer->stencilbuffer_id,
        framebuffer->colorbuffer_mode,
        framebuffer->width,
        framebuffer->height,
        framebuffer->bin_queue_id
    );

    // printf("Flushed draw state: framebuffer_id=%zu, context_id=%zu, triangles=%zu\n", framebuffer - orch->framebuffers, framebuffer->context_id, framebuffer->draw_state.assembled_triangles);
    
    framebuffer->draw_state.assembled_triangles  = 0;
    framebuffer->draw_state.assembled_vertices   = 0;
    framebuffer->clear_state.enabled = (enabled_data_t) {0};

}

static void __orch_flush_clear_state(orch_handler_t* orch, orch_framebuffer_handler_t* framebuffer)
{
    if (framebuffer->clear_state.enabled.misc == 0) return;
    
    device_launch_clear_framebuffer(
        &orch->device,
        framebuffer->clear_state.data,
        framebuffer->clear_state.enabled,
        framebuffer->colorbuffer_id,
        framebuffer->depthbuffer_id,
        framebuffer->stencilbuffer_id,
        framebuffer->colorbuffer_mode,
        framebuffer->width,
        framebuffer->height,
        framebuffer->bin_queue_id
    );
    
    framebuffer->clear_state.enabled = (enabled_data_t) {0};
}

static void __orch_flush_framebuffer(orch_handler_t* orch, orch_framebuffer_handler_t* framebuffer)
{
    __orch_flush_draw_state(orch, framebuffer);
    __orch_flush_clear_state(orch, framebuffer);
}

static void __orch_deattach_context(orch_handler_t* orch, orch_framebuffer_handler_t* framebuffer)
{
    if (framebuffer->context_id != -1)
    {
        __orch_flush_framebuffer(orch, framebuffer);

        orch->context_framebuffer_attachments[framebuffer->context_id] = NULL;

        framebuffer->context_id = -1;
        framebuffer->draw_state = (orch_draw_state_t) {
            .assembled_triangles = 0,
            .assembled_vertices = 0,
            .pending_vertices = 0
        };
        framebuffer->loaded_configs = 0;
    }
}

static device_context_t* __orch_attach_new_context(
    orch_handler_t* orch, 
    orch_framebuffer_handler_t* framebuffer,
    int move_fragment_data
) {
    device_context_t* prev_context  = __orch_get_attached_context(orch, framebuffer);

    size_t prev_loaded_configs = framebuffer->loaded_configs;

    size_t a = framebuffer->loaded_configs;
    
    size_t context_id = __orch_get_next_context_id(orch);

    orch_framebuffer_handler_t* prev_framebuffer = __orch_get_framebuffer_attached_to_context_id(orch, context_id);

    if (prev_framebuffer != NULL) 
    {
        __orch_deattach_context(orch, prev_framebuffer);
    }
    
    __orch_deattach_context(orch, framebuffer);

    device_context_t* context = __orch_get_context_from_id(orch, context_id);

    if (prev_context != NULL && context != NULL) { // context nullability is checked so compiler does no complain

        if (context != prev_context) 
        {
            device_copy_context_last_state(context, prev_context);
        } 

        if (move_fragment_data && prev_loaded_configs > 0)
        {
            device_copy_fragment_state(context, prev_context, framebuffer->loaded_configs, prev_loaded_configs - 1);
            framebuffer->loaded_configs += 1;
        }
    }

    framebuffer->context_id = context_id;
    orch->context_framebuffer_attachments[context_id] = framebuffer;

    return context;
}

/*
static uint8_t orch_require_flush_clear(orch_framebuffer_handler_t* framebuffer, clear_data_t data, enabled_data_t enabled)
{
    uint32_t red_enabled = get_enabled_red_data(framebuffer->clear_state.enabled) && get_enabled_red_data(enabled);
    uint32_t red_diff = get_rgba8_red(framebuffer->clear_state.data.color) !=  get_rgba8_red(data.color); 
    
    if (red_enabled && red_diff) return 1;

    uint32_t green_enabled = get_enabled_green_data(framebuffer->clear_state.enabled) && get_enabled_green_data(enabled);
    uint32_t green_diff = get_rgba8_green(framebuffer->clear_state.data.color) !=  get_rgba8_green(data.color);
    
    if (green_enabled && green_diff) return 1;

    uint32_t blue_enabled = get_enabled_blue_data(framebuffer->clear_state.enabled) && get_enabled_blue_data(enabled);
    uint32_t blue_diff = get_rgba8_blue(framebuffer->clear_state.data.color) !=  get_rgba8_blue(data.color);
    
    if (blue_enabled && blue_diff) return 1;

    uint32_t alpha_enabled = get_enabled_alpha_data(framebuffer->clear_state.enabled) && get_enabled_alpha_data(enabled);
    uint32_t alpha_diff = get_rgba8_alpha(framebuffer->clear_state.data.color) !=  get_rgba8_alpha(data.color);
    
    if (alpha_enabled && alpha_diff) return 1;

    uint32_t depth_enabled = get_enabled_depth_data(framebuffer->clear_state.enabled) && get_enabled_depth_data(enabled);
    uint32_t depth_diff = framebuffer->clear_state.data.depth.misc != framebuffer->clear_state.data.depth.misc;
    
    if (depth_enabled && depth_diff) return 1;

    uint32_t stencil_enabled = get_enabled_stencil_data(framebuffer->clear_state.enabled) && get_enabled_stencil_data(enabled);
    uint32_t stencil_diff = framebuffer->clear_state.data.stencil.misc !=  framebuffer->clear_state.data.stencil.misc;
    
    if (stencil_enabled && stencil_diff) return 1;

    return 0;
}
*/

static void __orch_draw_vertices(
    orch_handler_t* orch,
    orch_framebuffer_handler_t* framebuffer,
    size_t shader_id,
    render_mode_t mode,
    uint32_t init,
    uint32_t end
) {
    size_t num_vertices = end - init;
    size_t num_triangles = __orch_get_num_triangles_from_vertices(mode, num_vertices);

    device_context_t* context = __orch_get_attached_or_attach_context(orch, framebuffer);

    __orch_flush_context_reason_t flush_context_reason = __orch_require_flush_context(framebuffer, shader_id, mode, num_vertices); 
    if (flush_context_reason)
    {
        context = __orch_attach_new_context(orch, framebuffer, 1);

        if (flush_context_reason == TRIANGLE_BUFFER_CAPACITY && num_triangles > DEVICE_MAX_NUMBER_TRIANGLES)
        {
            printf("ERROR: Do not supported triangle size draw call. num triangles => %ld > %ld\n", num_triangles, (size_t) DEVICE_MAX_NUMBER_TRIANGLES);
            exit(1);
        }

        if (flush_context_reason == VERTEX_BUFFER_CAPACITY && num_vertices > DEVICE_VERTICES_SIZE)
        {
            printf("ERROR: Do not supported vertices size draw call. num vertices => %ld > %ld\n", num_vertices, (size_t) DEVICE_VERTICES_SIZE);
            exit(1);
        }
    }

    if (__orch_require_flush_vertices(framebuffer, mode, num_vertices))
    {
        __orch_flush_vertices(orch, framebuffer);
    }

    size_t last_config_id = framebuffer->loaded_configs - 1;

    device_launch_vertex_shader(
        context,
        shader_id,
        init, 
        end,
        framebuffer->draw_state.assembled_vertices,
        last_config_id,
        1
    );

    framebuffer->draw_state.last_render_mode = mode;
    framebuffer->draw_state.last_config_id = last_config_id;
    framebuffer->draw_state.assembled_vertices += num_vertices;
    framebuffer->draw_state.pending_vertices += num_vertices;
    framebuffer->draw_state.shader_id = shader_id;
}

static void __orch_merge_clear_state(orch_framebuffer_handler_t* framebuffer, clear_data_t data, enabled_data_t enabled)
{
    enabled_data_t *enabled_ptr = &framebuffer->clear_state.enabled;

    // color merge
    rgba8_t *color_ptr = &framebuffer->clear_state.data.color;

    if (get_enabled_red_data(enabled)) {
        set_enabled_data_red_enable(enabled_ptr);
        set_rgba8_red(color_ptr, get_rgba8_red(data.color));
    }

    if (get_enabled_green_data(enabled)) {
        set_enabled_data_green_enable(enabled_ptr);
        set_rgba8_green(color_ptr, get_rgba8_green(data.color));
    }

    if (get_enabled_blue_data(enabled)) {
        set_enabled_data_blue_enable(enabled_ptr);
        set_rgba8_blue(color_ptr, get_rgba8_blue(data.color));
    }

    if (get_enabled_alpha_data(enabled)) {
        set_enabled_data_alpha_enable(enabled_ptr);
        set_rgba8_alpha(color_ptr, get_rgba8_alpha(data.color));
    }

    // depth merge
    depth16_t *depth_ptr = &framebuffer->clear_state.data.depth;
    if (get_enabled_depth_data(enabled)) {
        set_enabled_depth_data(enabled_ptr, 1);
        *depth_ptr = data.depth;
    }

    // stencil merge
    // TODO: use an interface
    stencil8_t *stencil_ptr = &framebuffer->clear_state.data.stencil;
    cl_uchar old_stencil_mask = get_enabled_stencil_data(*enabled_ptr);
    cl_uchar new_stencil_mask = get_enabled_stencil_data(enabled);
    set_enabled_stencil_data(enabled_ptr, old_stencil_mask | new_stencil_mask);
    stencil_ptr->misc |= data.stencil.misc & new_stencil_mask;
}



//--------------------------------------------------------------------------
// Public methods
//--------------------------------------------------------------------------

// Init functions

void orch_init(orch_handler_t* orch)
{
    orch->next_context_id = 0;
    orch->framebuffer_size = 0;
    orch->rw_image2d_size = 0;

    device_init(&orch->device);

    for(size_t context_id = 0; context_id < DEVICE_CONTEXT_NUMBER; ++context_id)
    {
        device_init_context(&orch->contexts[context_id], &orch->device);
        orch->context_framebuffer_attachments[context_id] = NULL;
    }
}

// Create functions

size_t orch_create_framebuffer(orch_handler_t* orch)
{
    if (orch->framebuffer_size == HOST_FRAMEBUFFER_SIZE) exit(0);

    orch_framebuffer_handler_t* framebuffer = &orch->framebuffers[orch->framebuffer_size];
    
    size_t bin_queue_id = device_create_bin_queue(&orch->device);

    *framebuffer = (orch_framebuffer_handler_t) {
        .draw_state = {
            .shader_id = 0,
            .assembled_triangles = 0,
            .assembled_vertices = 0,
            .pending_vertices = 0,
            .last_config_id = 0,
            .last_render_mode = 0,
        },
        .clear_state = {
            .data = 0,
            .enabled = 0,
        },
        .context_id = -1,
        .loaded_configs = 0,
        .colorbuffer_id = 0,
        .depthbuffer_id = 0,
        .stencilbuffer_id = 0,
        .colorbuffer_mode = 0,
        .width = 0,
        .height = 0,
        .bin_queue_id = bin_queue_id,
    };

    size_t framebuffer_id = orch->framebuffer_size;
    orch->framebuffer_size += 1;

    return  framebuffer_id;
}

size_t orch_create_image2d(orch_handler_t* orch, size_t width, size_t height, uint32_t mode)
{
    if (orch->rw_image2d_size == HOST_IMAGES_REF_SIZE) exit(0);

    orch_rw_image2d_handler_t* image = &orch->rw_image2ds[orch->rw_image2d_size];

    size_t device_image_id = device_create_2d_texture(&orch->device, width, height, mode);

    *image = (orch_rw_image2d_handler_t) {
        .width = width,
        .height = height,
        .mode = mode,
        .image_id = device_image_id
    };

    size_t rw_image2d_id = orch->rw_image2d_size;
    orch->rw_image2d_size += 1;

    return rw_image2d_id;
}

size_t orch_create_renderbuffer(orch_handler_t* orch, size_t width, size_t height, uint32_t mode)
{
    return orch_create_image2d(orch, width, height, mode);
}

size_t orch_create_2d_texture(orch_handler_t* orch, size_t width, size_t height, uint32_t mode)
{
    return orch_create_image2d(orch, width, height, mode);
}

size_t orch_create_shader_from_binary(orch_handler_t* orch, size_t lenght, const void* binary)
{
    return device_create_program_from_binary(&orch->device, lenght, (const unsigned char*) binary);
}

size_t orch_create_buffer(orch_handler_t* orch, size_t size)
{
    return device_create_ro_buffer(&orch->device, size);
}

// Get functions

size_t orch_get_shader_attribute_size(orch_handler_t* orch, size_t shader_id)
{
    return device_get_program_vertex_attrib_size(&orch->device, shader_id);
}

size_t orch_get_shader_uniform_size(orch_handler_t* orch, size_t shader_id)
{
    return device_get_program_uniform_size(&orch->device, shader_id);
}

void orch_get_shader_attribute_arg_data(orch_handler_t* orch, size_t shader_id, size_t location, arg_data_t *arg_data)
{
    device_get_program_vertex_attrib_arg_data(&orch->device, shader_id, location, arg_data);
}

void orch_get_shader_uniform_arg_data(orch_handler_t* orch, size_t shader_id, size_t location, arg_data_t *arg_data)
{
    device_get_program_uniform_arg_data(&orch->device, shader_id, location, arg_data);
}

// Attach functions

void orch_attach_render_colorbuffer(orch_handler_t* orch, size_t framebuffer_id, size_t color_id)
{
    __orch_attach_rw_image2d_colorbuffer(orch, framebuffer_id, color_id);
}

void orch_attach_render_depthbuffer(orch_handler_t* orch, size_t framebuffer_id, size_t depth_id)
{
    __orch_attach_rw_image2d_depthbuffer(orch, framebuffer_id, depth_id);
}

void orch_attach_render_stencilbuffer(orch_handler_t* orch, size_t framebuffer_id, size_t stencil_id)
{
    __orch_attach_rw_image2d_stencilbuffer(orch, framebuffer_id, stencil_id);
}

void orch_attach_texture_colorbuffer(orch_handler_t* orch, size_t framebuffer_id, size_t color_id)
{
    __orch_attach_rw_image2d_colorbuffer(orch, framebuffer_id, color_id);
}

void orch_attach_texture_depthbuffer(orch_handler_t* orch, size_t framebuffer_id, size_t depth_id)
{
    __orch_attach_rw_image2d_depthbuffer(orch, framebuffer_id, depth_id);
}

void orch_attach_texture_stencilbuffer(orch_handler_t* orch, size_t framebuffer_id, size_t stencil_id)
{
    __orch_attach_rw_image2d_stencilbuffer(orch, framebuffer_id, stencil_id);
}

void orch_attach_vertex_attribute_ptr(orch_handler_t* orch, size_t framebuffer_id, uint32_t attribute, size_t buffer_id)
{
    orch_framebuffer_handler_t* framebuffer = __orch_get_framebuffer_from_id(orch, framebuffer_id);

    device_context_t* context = __orch_get_attached_or_attach_context(orch, framebuffer);

    device_bind_buffer_to_vertex_attribute_pointer(context, attribute, buffer_id);
}

void orch_attach_vertex_attribute_host_ptr(
    orch_handler_t* orch, 
    size_t framebuffer_id, 
    uint32_t attribute, 
    size_t stride, 
    void* ptr
) {
    orch_framebuffer_handler_t* framebuffer = __orch_get_framebuffer_from_id(orch, framebuffer_id);

    device_context_t* context = __orch_get_attached_or_attach_context(orch, framebuffer);

    device_bind_host_pointer_to_vertex_attribute_pointer(context, attribute, stride, ptr);
}

void orch_attach_texture_unit(
    orch_handler_t* orch, 
    size_t framebuffer_id, 
    size_t texture_unit, 
    size_t texture_id
) {
    orch_framebuffer_handler_t* framebuffer = __orch_get_framebuffer_from_id(orch, framebuffer_id);

    device_context_t* context = __orch_get_attached_or_attach_context(orch, framebuffer);

    orch_rw_image2d_handler_t* image2d = &orch->rw_image2ds[texture_id];

    device_bind_texture_unit(context, texture_unit, image2d->image_id);
}
// Write functions

void orch_write_buffer(
    orch_handler_t* orch, 
    size_t buffer_id,
    size_t offset,
    size_t size, 
    const void* data
) {
    device_write_buffer(&orch->device, buffer_id, offset, size, data);
}

void orch_write_2d_texture(
    orch_handler_t* orch, 
    size_t texture_id,
    size_t x,
    size_t y,
    size_t width, 
    size_t height, 
    uint32_t mode,
    const void* data
) {
    orch_rw_image2d_handler_t* image2d = &orch->rw_image2ds[texture_id];

    device_write_2d_texture(
        &orch->device, 
        image2d->image_id, 
        x, y,
        width, height, 
        image2d->mode,
        mode, 
        data
    );
}

void orch_write_vertex_attributes(
    orch_handler_t* orch, 
    size_t framebuffer_id, 
    float vertex_attributes[DEVICE_VERTEX_ATTRIBUTE_SIZE][4]
) {
    orch_framebuffer_handler_t* framebuffer = __orch_get_framebuffer_from_id(orch, framebuffer_id);

    device_context_t* context = __orch_get_attached_or_attach_context(orch, framebuffer);

    device_write_vertex_attributes(context, vertex_attributes, 1);
}

void orch_write_vertex_attribute_data(
    orch_handler_t* orch, 
    size_t framebuffer_id, 
    vertex_attribute_data_t* data
) {
    orch_framebuffer_handler_t* framebuffer = __orch_get_framebuffer_from_id(orch, framebuffer_id);

    device_context_t* context = __orch_get_attached_or_attach_context(orch, framebuffer);

    device_write_vertex_attribute_data(context, data);
}

void orch_write_fragment_texture_data(
    orch_handler_t* orch,
    size_t framebuffer_id,
    texture_data_t texture_data[DEVICE_TEXTURE_UNITS]
) {
    orch_framebuffer_handler_t* framebuffer = __orch_get_framebuffer_from_id(orch, framebuffer_id);
    
    device_context_t* context = __orch_get_attached_or_attach_context(orch, framebuffer);

    __orch_flush_draw_state(orch, framebuffer);

    device_write_fragment_texture_datas(context, texture_data);
}

void orch_write_fragment_data(
    orch_handler_t* orch, 
    size_t framebuffer_id, 
    uint8_t uniform_data[DEVICE_UNIFORM_CAPACITY],
    rop_config_t config
) {
    orch_framebuffer_handler_t* framebuffer = __orch_get_framebuffer_from_id(orch, framebuffer_id);

    device_context_t* context = __orch_get_attached_or_attach_context(orch, framebuffer);

    if (framebuffer->loaded_configs == TRIANGLE_PRIMITIVE_CONFIGS)
    {
        __orch_attach_new_context(orch, framebuffer, 0);
    }

    device_write_fragment_uniform(context, framebuffer->loaded_configs, uniform_data);
    device_write_rop_config(context, framebuffer->loaded_configs, &config);
    
    framebuffer->loaded_configs += 1;
}

// Work functions

void orch_draw_arrays(
    orch_handler_t* orch, 
    size_t framebuffer_id,
    size_t shader_id, 
    render_mode_t mode, 
    uint32_t init, 
    uint32_t end
) {
    orch_framebuffer_handler_t* framebuffer = __orch_get_framebuffer_from_id(orch, framebuffer_id);

    __orch_draw_vertices(orch, framebuffer, shader_id, mode, init, end);

    #if (DEVICE_ORCHESTRATOR_ENABLED == 0)
    {
        __orch_flush_framebuffer(orch, framebuffer);
    }
    #endif
}

void orch_draw_range(
    orch_handler_t* orch, 
    size_t framebuffer_id,
    size_t shader_id, 
    render_mode_t mode, 
    uint32_t init,
    uint32_t end, 
    uint32_t count, 
    const uint16_t* ptr
) {
    orch_framebuffer_handler_t* framebuffer = __orch_get_framebuffer_from_id(orch, framebuffer_id);

    __orch_draw_vertices(orch, framebuffer, shader_id, mode, init, end);

    if (framebuffer->draw_state.pending_vertices == 0) return;

    size_t pending_triangles = __orch_get_num_triangles_from_vertices(
        framebuffer->draw_state.last_render_mode, 
        count
    );

    size_t vertices_offset = framebuffer->draw_state.assembled_vertices - framebuffer->draw_state.pending_vertices;

    device_context_t* context = __orch_get_attached_or_attach_context(orch, framebuffer);

    device_launch_range_triangle_assembly(
        context,
        framebuffer->draw_state.shader_id,
        framebuffer->draw_state.last_render_mode, 
        framebuffer->draw_state.last_config_id,
        vertices_offset,
        framebuffer->draw_state.assembled_triangles,
        pending_triangles,
        framebuffer->width,
        framebuffer->height,
        count,
        ptr
    );

    framebuffer->draw_state.pending_vertices = 0;
    framebuffer->draw_state.assembled_triangles += pending_triangles;

    #if (DEVICE_ORCHESTRATOR_ENABLED == 0)
    {
        __orch_flush_framebuffer(orch, framebuffer);
    }
    #endif
}

void orch_clear(
    orch_handler_t* orch,
    size_t framebuffer_id,
    clear_data_t data,
    enabled_data_t enabled
) {
    orch_framebuffer_handler_t* framebuffer = __orch_get_framebuffer_from_id(orch, framebuffer_id);

    __orch_flush_draw_state(orch, framebuffer);

    __orch_merge_clear_state(framebuffer, data, enabled);
}

void orch_readnpixels(
    orch_handler_t* orch,
    size_t framebuffer_id,
    size_t x, size_t y,
    size_t width, size_t height,
    uint32_t mode,
    int swap_rb,
    int swap_y,
    void* ptr
) {
    orch_framebuffer_handler_t* framebuffer = __orch_get_framebuffer_from_id(orch, framebuffer_id);

    __orch_flush_framebuffer(orch, framebuffer);

    if (framebuffer->width != width || framebuffer->height != height) NOT_IMPLEMENTED;

    device_launch_read_pixels(
        &orch->device,
        framebuffer->bin_queue_id,
        framebuffer->colorbuffer_id,
        framebuffer->colorbuffer_mode,
        x, y, width, height, 
        mode, 
        swap_rb, 
        swap_y,
        ptr
    );
}

void orch_flush(
    orch_handler_t* orch, 
    size_t framebuffer_id
) {
    orch_framebuffer_handler_t* framebuffer = __orch_get_framebuffer_from_id(orch, framebuffer_id);
 
    __orch_flush_framebuffer(orch, framebuffer);
}

void orch_finish(
    orch_handler_t* orch, 
    size_t framebuffer_id
) {
    orch_framebuffer_handler_t* framebuffer = __orch_get_framebuffer_from_id(orch, framebuffer_id);

    __orch_flush_framebuffer(orch, framebuffer);
    device_wait_bin_queue(&orch->device, framebuffer->bin_queue_id);
}

void orch_destroy(orch_handler_t* orch) NOT_IMPLEMENTED;