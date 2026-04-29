#ifndef MIDDLEWARE_DEVICE_H
#define MIDDLEWARE_DEVICE_H

#include <types.device.h>

// TODO: impl structs on impl file not on header

typedef struct {
    cl_kernel kernel;
    uint32_t  number_attributes;
    uint32_t  vertex_size;
} __device_vertex_shader_data_t;

typedef struct {
    __device_vertex_shader_data_t vertex;
    cl_kernel fragment, uniform_data, attribute_data, varying_data;
} __device_shader_kernels_t;

typedef struct {
    cl_command_queue queues[DEVICE_BIN_QUEUE_SIZE][DEVICE_BIN_QUEUE_SIZE]; // TODO: multiple queues
} __device_bin_queue_t;

typedef union {
    size_t  device_id;
    void*   host;
} __device_vertex_attribute_pointer_mem_u;

typedef struct {
    union {
        size_t is_host;
        size_t stride;
    };
    __device_vertex_attribute_pointer_mem_u mem;
} __device_vertex_attribute_pointer_t;

typedef struct {
    cl_mem              mem;
    cl_command_queue    queue;
    cl_event            write_event;
} __device_mem_t;


// TODO: device textures is hw dependant
typedef struct {
    __device_mem_t mem;
    #ifdef DEVICE_IMAGE_ENABLED
        cl_sampler sampler;
        #ifndef DEVICE_RW_IMAGE_ENABLED
            cl_mem buffer_mem;
        #endif
    #endif
} __device_texture_t;

typedef struct {
    unsigned int vertex_location, fragment_location, size, type, offset;
    char name[128];
} arg_data_t;

typedef struct {
    cl_platform_id              platform_id;
    cl_device_id                device_id;
    cl_context                  context;

    cl_program                  triangle_setup_program;
    cl_program                  bin_raster_program;
    cl_program                  coarse_raster_program;
    cl_program                  clear_program;
    cl_program                  read_pixels_program;

    cl_kernel                   bin_raster_kernel;
    cl_kernel                   clear_kernel;
    cl_kernel                   coarse_raster_kernel;
    cl_kernel                   read_pixels_kernel;
    cl_kernel                   triangle_setup_arrays_kernel;
    cl_kernel                   triangle_setup_range_kernel;

    size_t                      programs_size;
    cl_program                  programs            [HOST_PROGRAMS_SIZE];
    __device_shader_kernels_t     shaders             [HOST_PROGRAMS_SIZE];

    size_t                      bin_queues_size;
    __device_bin_queue_t                 bin_queues          [HOST_FRAMEBUFFER_SIZE];

    // host writes are waited, so there is no need to sync 
    size_t                      buffers_size;
    __device_mem_t              buffers             [HOST_BUFFERS_SIZE];

    size_t                      textures_size;
    __device_mem_t              textures            [HOST_TEXTURES_SIZE];
} device_t;

typedef struct {
    device_t *device;

    // Buffers
    cl_mem g_tri_header;        
    cl_mem g_tri_data;
    cl_mem g_tri_subtris;
    cl_mem g_vertex_buffer;

    cl_mem g_bin_first_seg; 
    cl_mem g_bin_seg_data; 
    cl_mem g_bin_seg_next;
    cl_mem g_bin_seg_count;
    cl_mem g_bin_total;

    cl_mem g_active_tiles;
    cl_mem g_tile_first_seg;
    cl_mem g_tile_seg_data;
    cl_mem g_tile_seg_next;
    cl_mem g_tile_seg_count;

    // Atomics
    __device_mem_t a_bin_counter;
    __device_mem_t a_coarse_counter;
    __device_mem_t a_fine_counter;
    __device_mem_t a_num_active_tiles;
    __device_mem_t a_num_bin_segs;
    __device_mem_t a_num_subtris;
    __device_mem_t a_num_tile_segs;

    #ifdef DEVICE_IMAGE_ENABLED
    // Images
    cl_mem t_tri_data;
    cl_mem t_tri_header;
    cl_mem t_vertex_buffer;
    #endif

    // Binded objects from device
    __device_vertex_attribute_pointer_t     vertex_attribute_pointers  [DEVICE_VERTEX_ATTRIBUTE_SIZE];
    size_t                                  texture_units_ids          [DEVICE_TEXTURE_UNITS];

    // Vertex async state
    // TODO: maybe unify into a OoO queue
    size_t                          vertex_command_queue_index;
    cl_command_queue                vertex_command_queues       [DEVICE_VERTEX_COMMAND_QUEUE_SIZE];

    size_t                          vertex_attributes_index; 
    __device_mem_t                  vertex_attributes_mem       [DEVICE_VERTEX_COMMAND_QUEUE_SIZE];     // sizeof(float[DEVICE_VERTEX_ATTRIBUTE_SIZE][4])

    size_t                          vertex_attribute_data_index; 
    __device_mem_t                  vertex_attribute_data_mem   [DEVICE_VERTEX_COMMAND_QUEUE_SIZE];     // sizeof(vertex_attribute_data_t[DEVICE_VERTEX_ATTRIBUTE_SIZE])

    size_t                          vertex_uniform_index;
    __device_mem_t                  vertex_uniform_mem          [DEVICE_VERTEX_COMMAND_QUEUE_SIZE];     // sizeof(cl_uchar[DEVICE_VERTEX_COMMAND_QUEUE_SIZE][DEVICE_UNIFORM_CAPACITY])            

    // TODO: this is not used, but could be implemented, only use frament_texture_datas_mem for now
    // cl_mem                  vertex_texture_datas_mem    [DEVICE_VERTEX_COMMAND_QUEUE_SIZE];     // sizeof(texture_data_t[DEVICE_VERTEX_COMMAND_QUEUE_SIZE][DEVICE_TEXTURE_UNITS])
    
    // Primitive Configuration
    // Raster async state
    cl_command_queue        raster_command_queue;
    cl_event                bin_wait_event                  [DEVICE_BIN_QUEUE_SIZE][DEVICE_BIN_QUEUE_SIZE];
    __device_mem_t          fragment_texture_datas_mem;                                          // sizeof(texture_data_t[DEVICE_TEXTURE_UNITS])
    __device_mem_t          fragment_uniform_mem;                                           // sizeof(cl_uchar[TRIANGLE_PRIMITIVE_CONFIGS][DEVICE_UNIFORM_CAPACITY])
    // cl_mem                  fragment_uniform_subbuffer_mems [TRIANGLE_PRIMITIVE_CONFIGS];    // subbuffers for vertex shader, TODO: do not use sub-buffers in OpenCL
    __device_mem_t          rop_configs_mem;                                                // sizeof(rop_config_t[TRIANGLE_PRIMITIVE_CONFIGS])

} device_context_t;

// device constructors & destructors

void device_init(device_t* device);

void device_destroy(device_t* device); 

// device creators

size_t device_create_bin_queue(device_t* device);

size_t device_create_program_from_binary(device_t* device, size_t size, const unsigned char* binary);

size_t device_create_ro_buffer(device_t* device, size_t size); 

size_t device_create_2d_texture(device_t* device, size_t width, size_t height, cl_uint texture_mode);

// device getters

void   device_get_program_uniform_arg_data(device_t* shared, size_t program_id, size_t location, arg_data_t* arg_data);

size_t device_get_program_uniform_size(device_t* device, size_t program_id);

void   device_get_program_vertex_attrib_arg_data(device_t* device, size_t program_id, size_t location, arg_data_t* arg_data);

size_t device_get_program_vertex_attrib_size(device_t* device, size_t program_id);

// device readers

void device_read_buffer(device_t* device, size_t buffer_id, size_t offset, size_t size, void* data);

// device work enqueuers

void device_launch_clear_framebuffer(
    device_t* device,
    clear_data_t c_data,
    enabled_data_t c_enabled,
    size_t colorbuffer_id, size_t depthbuffer_id, size_t stencilbuffer_id,
    uint32_t colorbuffer_mode,
    size_t width, size_t height, 
    size_t bin_queue_id
);

void device_launch_read_pixels(
    device_t* device,
    size_t bin_queue_id,
    size_t colorbuffer_id,
    uint32_t colorbuffer_mode,
    size_t x, size_t y,
    size_t width, size_t height,
    uint32_t ptr_mode,
    uint32_t swap_rb,
    uint32_t swap_y,
    void* ptr
);

// device work synchronizers

void device_wait_bin_queue(device_t* device, size_t bin_queue_id);

// device writers

void device_write_2d_texture(
    device_t* device, 
    size_t texture_id, 
    size_t x, 
    size_t y, 
    size_t width,
    size_t height,
    uint32_t device_texture_mode,
    uint32_t host_texture_mode,
    const void* data
);

void device_write_buffer(device_t* device, size_t buffer_id, size_t offset, size_t size, const void* data);

// ---------------------------------------------------------------------------------------------

// device context constructors & destructors

void device_init_context(device_context_t *context, device_t* device);

void device_destroy_context(device_context_t* context); 

// device context binders

void device_bind_buffer_to_vertex_attribute_pointer(
    device_context_t* context, 
    size_t attribute_id, 
    size_t buffer_id
);

void device_bind_host_pointer_to_vertex_attribute_pointer(
    device_context_t* context, 
    size_t attribute_id,
    size_t stride,
    void* ptr
);

void device_bind_texture_unit(device_context_t* context, size_t unit_index, size_t texture_id);

// device context copiers

void device_copy_fragment_state(device_context_t* dst, device_context_t* src, size_t dst_id, size_t src_id);

void device_copy_context_last_state(device_context_t* dst, device_context_t* src);

// device context work enqueuers

void device_launch_vertex_shader(
    device_context_t* context,
    size_t shader_id,
    size_t init,
    size_t end,
    size_t offset,
    size_t primitive_id,
    int use_fragment_uniform
);

void device_launch_range_triangle_assembly(
    device_context_t* context, 
    size_t shader_id,
    render_mode_t mode, 
    size_t frag_config_id, 
    size_t vertex_offset, size_t triangle_offset, size_t num_triangles,
    size_t width, size_t height, 
    size_t size, const uint16_t* ptr
);

void device_launch_arrays_triangle_assembly(
    device_context_t* context, 
    size_t shader_id,
    render_mode_t mode, 
    size_t frag_config_id, 
    size_t vertex_offset, 
    size_t triangle_offset, 
    size_t num_triangles,
    size_t width, 
    size_t height 
);

void device_launch_bin_dispatch(
    device_context_t* context, 
    size_t num_triangles, 
    size_t width, 
    size_t height
);

void device_launch_tile_dispatch(
    device_context_t* context,
    uint32_t deferred_clear,
    size_t width, 
    size_t height
);

void device_launch_fragment_shader(
    device_context_t* context,
    size_t shader_id,
    clear_data_t c_data,
    enabled_data_t c_enabled,
    size_t colorbuffer_id, size_t depthbuffer_id, size_t stencilbuffer_id,
    uint32_t colorbuffer_mode,
    size_t width, size_t height, 
    size_t bin_queue_id
);

// device context writers
// TODO: write api return an event handler, to do async writing

// TODO: textures are not implemented for vertex
void device_write_fragment_texture_datas(
    device_context_t* context, 
    const texture_data_t texture_datas[DEVICE_TEXTURE_UNITS]
);

void device_write_fragment_uniform(
    device_context_t* context,
    size_t primitive_id,
    const uint8_t uniform_data[DEVICE_UNIFORM_CAPACITY]
);

void device_write_rop_config(
    device_context_t* context, 
    size_t primitive_id, 
    rop_config_t* rop_config
);

void device_write_vertex_attribute_data(
    device_context_t* context,
    vertex_attribute_data_t vertex_attribute_data[DEVICE_VERTEX_ATTRIBUTE_SIZE]
);

void device_write_vertex_attributes(
    device_context_t* context, 
    float vertex_attributes[DEVICE_VERTEX_ATTRIBUTE_SIZE][4],
    int blocking_write
);

void device_write_vertex_uniform(
    device_context_t* context, 
    uint8_t uniform_data[DEVICE_UNIFORM_CAPACITY],
    int blocking_write
);

#endif // MIDDLEWARE_DEVICE_H
