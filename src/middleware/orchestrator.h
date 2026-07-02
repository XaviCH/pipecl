#ifndef MIDDLEWARE_ORCHESTRATOR_H
#define MIDDLEWARE_ORCHESTRATOR_H

#include <middleware/device.h>

// TODO: impl structs on impl file not on header

// TODO: clock orchetrator to evaluate performance and potential optimizations
/*
typedef struct {
    clock_t init_clock, destroy_clock;
} __orch_debug_t;
*/

typedef struct {
    size_t shader_id;
    size_t assembled_triangles;
    size_t assembled_vertices;
    size_t pending_vertices;
    size_t pending_triangles;

    size_t last_config_id;
    render_mode_t last_render_mode;
} orch_draw_state_t;

typedef struct {
    clear_data_t data;
    enabled_data_t enabled;
} orch_clear_state_t;

typedef struct {
    uint32_t width, height;
    uint32_t mode;
    size_t image_id;
} orch_rw_image2d_handler_t;

typedef struct {
    orch_draw_state_t draw_state;
    orch_clear_state_t clear_state;
    size_t context_id;
    size_t loaded_configs;
    // framebuffer data
    size_t colorbuffer_id, depthbuffer_id, stencilbuffer_id; // 0 is the dummy one
    uint32_t colorbuffer_mode;
    uint32_t width, height;
    size_t bin_queue_id;

    device_event_t vertex_attribute_data_write_event;
    device_event_t fragment_texture_data_write_event;

    uint8_t        uniform_staging              [TRIANGLE_PRIMITIVE_CONFIGS][DEVICE_UNIFORM_CAPACITY];
    rop_config_t   rop_config_staging           [TRIANGLE_PRIMITIVE_CONFIGS];
    device_event_t fragment_uniform_write_event [TRIANGLE_PRIMITIVE_CONFIGS];
    device_event_t rop_config_write_event       [TRIANGLE_PRIMITIVE_CONFIGS];

    uint8_t        use_vertex_uniform;
    size_t         vertex_uniform_staging_slot;
    uint8_t        vertex_uniform_staging       [DEVICE_VERTEX_COMMAND_QUEUE_SIZE][DEVICE_UNIFORM_CAPACITY];
    device_event_t vertex_uniform_write_event   [DEVICE_VERTEX_COMMAND_QUEUE_SIZE];
} orch_framebuffer_handler_t;

typedef struct {
    device_t device;

    size_t framebuffer_size;
    orch_framebuffer_handler_t framebuffers[HOST_FRAMEBUFFER_SIZE];
    
    size_t rw_image2d_size;
    orch_rw_image2d_handler_t rw_image2ds[HOST_IMAGES_REF_SIZE];
    
    size_t next_context_id;
    device_context_t contexts[DEVICE_CONTEXT_NUMBER];
    orch_framebuffer_handler_t* context_framebuffer_attachments[DEVICE_CONTEXT_NUMBER];

    uint8_t  batch_current_uniform [DEVICE_UNIFORM_CAPACITY];
    uint8_t  batch_arena           [DEVICE_MAX_BATCH_DRAWS][DEVICE_UNIFORM_CAPACITY];
    uint32_t batch_draw_start      [DEVICE_MAX_BATCH_DRAWS + 1];
    setup_draw_config_t batch_setup_config [DEVICE_MAX_BATCH_DRAWS + 1];
    vertex_config_t batch_config [DEVICE_MAX_BATCH_DRAWS];
    vertex_attribute_data_t batch_current_attr_data [DEVICE_VERTEX_ATTRIBUTE_SIZE];
    vertex_attribute_data_t batch_attr_data_arena   [DEVICE_MAX_BATCH_DRAWS][DEVICE_VERTEX_ATTRIBUTE_SIZE];
    float    batch_current_vertex_attributes [DEVICE_VERTEX_ATTRIBUTE_SIZE][4];
    float    batch_vertex_attributes_arena   [DEVICE_MAX_BATCH_DRAWS][DEVICE_VERTEX_ATTRIBUTE_SIZE][4];
    texture_data_t batch_current_texture_data [DEVICE_TEXTURE_UNITS];
    uint8_t  batch_texture_data_valid;
    size_t   batch_attr_buffer [DEVICE_VERTEX_ATTRIBUTE_SIZE];
    size_t   batch_num_draws;
    size_t   batch_run_max_draw_verts;
    size_t   batch_run_first_draw;
    uint8_t  batch_run_setup_flat;
    device_event_t batch_upload_event[DEVICE_MAX_BATCH_DRAWS];
} orch_handler_t;

// orchestrator constructors & destructors

void orch_init(orch_handler_t* orch);

void orch_destroy(orch_handler_t* orch);

// orchestrator creators

size_t orch_create_framebuffer(orch_handler_t* orch);

size_t orch_create_renderbuffer(orch_handler_t* orch, size_t width, size_t height, uint32_t mode);

size_t orch_create_2d_texture(orch_handler_t* orch, size_t width, size_t height, size_t levels, uint32_t mode);

void orch_generate_mipmap(orch_handler_t* orch, size_t texture_id, size_t levels);

size_t orch_create_shader_from_binary(orch_handler_t* orch, size_t lenght, const void* binary);

size_t orch_create_buffer(orch_handler_t* orch, size_t size);

// orchestrator getters

size_t orch_get_shader_attribute_size(orch_handler_t* orch, size_t shader_id);

size_t orch_get_shader_uniform_size(orch_handler_t* orch, size_t shader_id);

void orch_get_shader_attribute_arg_data(orch_handler_t* orch, size_t shader_id, size_t location, arg_data_t *arg_data);

void orch_get_shader_uniform_arg_data(orch_handler_t* orch, size_t shader_id, size_t location, arg_data_t *arg_data);

// orchestrator attachers

void orch_attach_render_colorbuffer(orch_handler_t* orch, size_t framebuffer_id, size_t color_id);

void orch_attach_render_depthbuffer(orch_handler_t* orch, size_t framebuffer_id, size_t depth_id);

void orch_attach_render_stencilbuffer(orch_handler_t* orch, size_t framebuffer_id, size_t stencil_id);

void orch_attach_texture_colorbuffer(orch_handler_t* orch, size_t framebuffer_id, size_t color_id);

void orch_attach_texture_depthbuffer(orch_handler_t* orch, size_t framebuffer_id, size_t depth_id);

void orch_attach_texture_stencilbuffer(orch_handler_t* orch, size_t framebuffer_id, size_t stencil_id);

void orch_attach_vertex_attribute_ptr(orch_handler_t* orch, size_t framebuffer_id, uint32_t attribute, size_t buffer_id);

void orch_attach_vertex_attribute_host_ptr(
    orch_handler_t* orch, 
    size_t framebuffer_id, 
    uint32_t attribute, 
    size_t stride, 
    void* ptr
);

void orch_attach_texture_unit(orch_handler_t* orch, size_t framebuffer_id, size_t texture_unit, size_t texture_id);

// orchestrator work enqueuers

void orch_draw_arrays(
    orch_handler_t* orch, 
    size_t framebuffer_id,
    size_t shader_id, 
    render_mode_t mode, 
    uint32_t init, 
    uint32_t end
);

void orch_draw_range(
    orch_handler_t* orch, 
    size_t framebuffer_id,
    size_t shader_id, 
    render_mode_t mode, 
    uint32_t init,
    uint32_t end, 
    uint32_t count, 
    const uint16_t* ptr
);

void orch_clear(orch_handler_t* orch, size_t framebuffer_id, clear_data_t data, enabled_data_t enabled);

void orch_readnpixels(
    orch_handler_t* orch,
    size_t framebuffer_id,
    size_t x, size_t y,
    size_t width, size_t height,
    uint32_t mode,
    int swap_rb,
    int swap_y,
    void* ptr
);

// orchestrator work synchronizers

void orch_flush(orch_handler_t* orch, size_t framebuffer_id);

void orch_finish(orch_handler_t* orch, size_t framebuffer_id);

// orchetrator writers

void orch_write_buffer(
    orch_handler_t* orch, 
    size_t buffer_id,
    size_t offset,
    size_t size, 
    const void* data
);

void orch_write_2d_texture(
    orch_handler_t* orch, 
    size_t texture_id,
    size_t x,
    size_t y,
    size_t width, 
    size_t height, 
    uint32_t mode,
    const void* data
);

void orch_write_vertex_attributes(
    orch_handler_t* orch, 
    float vertex_attributes[DEVICE_VERTEX_ATTRIBUTE_SIZE][4]
);

void orch_write_vertex_attribute_data(
    orch_handler_t* orch, 
    size_t framebuffer_id, 
    vertex_attribute_data_t* data
);

void orch_write_fragment_texture_data(
    orch_handler_t* orch,
    size_t framebuffer_id,
    texture_data_t texture_data[DEVICE_TEXTURE_UNITS]
);

void orch_write_fragment_data(
    orch_handler_t* orch,
    size_t framebuffer_id,
    uint8_t uniform_data[DEVICE_UNIFORM_CAPACITY],
    rop_config_t config
);

void orch_write_vertex_data(
    orch_handler_t* orch,
    uint8_t uniform_data[DEVICE_UNIFORM_CAPACITY]
);

#endif // MIDDLEWARE_ORCHESTRATOR_H