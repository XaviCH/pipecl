#include <middleware/device.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#if DEVICE_PERF_MODE
    #include <unistd.h>
    #include <time.h>
#endif

#define DEFINE_EXTERN_KERNEL(_NAME) \
    extern unsigned char _NAME##_o[]; \
    extern unsigned int _NAME##_o_len;

DEFINE_EXTERN_KERNEL(triangle_setup);
DEFINE_EXTERN_KERNEL(bin_raster);
DEFINE_EXTERN_KERNEL(coarse_raster);
DEFINE_EXTERN_KERNEL(force_clear);
DEFINE_EXTERN_KERNEL(readnpixels);

#undef DEFINE_EXTERN_KERNEL

// macros

#define NOT_IMPLEMENTED \
    { \
        printf("Funtion %s at %s:%d is not implemented.\n", __func__, __FILE__, __LINE__); \
        exit(1); \
    }

#ifdef NDEBUG
    #define CL_PANIC(_FUNC, _ERROR)
#else 
    #define CL_PANIC(_FUNC, _ERROR) \
    { \
        if (_ERROR != CL_SUCCESS) { \
            printf("OpenCL throw error at %s:%d. %s returned %d.\n", __FILE__, __LINE__, _FUNC, (int)_ERROR); \
            exit(_ERROR); \
        } \
    }
#endif

#define DEVICE_UNSUPORTED_MAPING(...) \
{                                                                       \
    printf("Device unsupported mapping at %s:%d. " __VA_ARGS__ "\n", __FILE__, __LINE__);    \
    exit(-1);                                                      \
}

#define CL_CHECK(...) \
{ \
    cl_int _error = __VA_ARGS__; \
    CL_PANIC(#__VA_ARGS__, _error) \
}

#define CL_ASSIGN_CHECK(_LEFT, ...) \
{ \
    cl_int _error; \
    cl_int *error = &_error;   \
    _LEFT = __VA_ARGS__; \
    CL_PANIC(#__VA_ARGS__, _error) \
}

#if DEVICE_PERF_MODE

    static double __device_now_ms(void)
    {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (double) ts.tv_sec * 1e3 + (double) ts.tv_nsec / 1e6;
    }

    static struct {
        int           initialized;
        int           enabled;
        unsigned long flushes;     // full draw-state flushes (bin+coarse+fine)
        unsigned long draws;       // draw calls issued
        double        last_report_s;
    } g_sync;

    static int __device_sync_enabled(void)
    {
        if (!g_sync.initialized)
        {
            const char* e = getenv("PIPECL_SYNC");
            g_sync.enabled = (e && e[0] && e[0] != '0');
            g_sync.last_report_s = __device_now_ms() / 1e3;
            g_sync.initialized = 1;
        }
        return g_sync.enabled;
    }

    static void __device_sync_tick(void)
    {
        if (!g_sync.enabled) return;
        double now_s = __device_now_ms() / 1e3;
        if (now_s - g_sync.last_report_s >= 1.0)
        {
            fprintf(stderr, "SYNC: flushes=%-5lu draws=%-6lu (%.1f draws/flush)\n",
                g_sync.flushes, g_sync.draws,
                g_sync.flushes ? (double) g_sync.draws / g_sync.flushes : 0.0);
            g_sync.flushes = 0; g_sync.draws = 0;
            g_sync.last_report_s = now_s;
        }
    }

    void device_sync_count_flush(void) { if (__device_sync_enabled()) g_sync.flushes += 1; }
    void device_sync_count_draw(void)  { if (__device_sync_enabled()) { g_sync.draws += 1; __device_sync_tick(); } }

#else // !DEVICE_PERF_MODE

    void device_sync_count_flush(void) {}
    void device_sync_count_draw(void)  {}

#endif // DEVICE_PERF_MODE

// methods

static size_t __device_get_sizeof_texel(cl_image_format* image_format)
{
    size_t size = 0;
    switch (image_format->image_channel_order) {
        case CL_R:
        case CL_A:
        case CL_DEPTH:
        case CL_INTENSITY:
            size = 1;
            break;
        case CL_RG:
        case CL_RA:
            size = 2;
            break;
        case CL_RGB:
            size = 3;
            break;
        case CL_RGBA:
        case CL_BGRA:
            size = 4;
            break;
    }

    switch (image_format->image_channel_data_type) {
        case CL_SNORM_INT8:
        case CL_UNORM_INT8:
        case CL_SIGNED_INT8:
        case CL_UNSIGNED_INT8:
            size *= 1;
            break;
        case CL_SNORM_INT16:
        case CL_UNORM_INT16:
        case CL_SIGNED_INT16:  
        case CL_UNSIGNED_INT16: 
            size *= 2;
            break;
        case CL_SIGNED_INT32:  
        case CL_UNSIGNED_INT32: 
            size *= 4;
            break; 
    }

    return size;
}

static size_t __device_get_sizeof_image(cl_image_format* image_format, cl_image_desc* image_desc)
{
    size_t texel_size = __device_get_sizeof_texel(image_format);

    return image_desc->image_width * image_desc->image_height * texel_size;
}

static void __device_init_mem(
    device_t* device,
    __device_mem_t* mem,
    cl_mem_flags flags,
    size_t size,
    void* host_ptr
) {
    CL_ASSIGN_CHECK(mem->queue, clCreateCommandQueue(device->context, device->device_id, 0, error)); 
    CL_ASSIGN_CHECK(mem->mem,   clCreateBuffer(device->context, flags, size, host_ptr, error));
    mem->write_event = NULL;
}

static void __device_init_2d_image(
    device_t* device,
    __device_mem_t* mem,
    cl_mem_flags flags,
    cl_image_format* image_format,
    cl_image_desc* image_desc,
    void* host_ptr
) {
    CL_ASSIGN_CHECK(mem->queue, clCreateCommandQueue(device->context, device->device_id, 0, error));

    #ifdef DEVICE_IMAGE_ENABLED
    {
        CL_ASSIGN_CHECK(mem->mem,   clCreateImage(device->context, flags, image_format, image_desc, host_ptr, error));
    }
    #else
    {
        // TODO: maybe sizeof_image should compute mip mapping size also
        size_t width  = image_desc->image_width;
        size_t height = image_desc->image_height;

        size_t size = width * height;
        for (cl_uint level = 1; level < image_desc->num_mip_levels; ++level)
        {
            width  = width  > 1 ? width  / 2 : 1;
            height = height > 1 ? height / 2 : 1;
            size += width * height;
        }
        size *= __device_get_sizeof_texel(image_format);

        CL_ASSIGN_CHECK(mem->mem,   clCreateBuffer(device->context, flags, size, host_ptr, error));
    }
    #endif

    mem->write_event = NULL;
}

static void __device_init_2d_texture_buffer(
    device_t* device,
    __device_mem_t* buffer_mem,
    cl_image_format* image_format,
    cl_image_desc* image_desc
) {
    size_t size = __device_get_sizeof_image(image_format, image_desc);
    CL_ASSIGN_CHECK(buffer_mem->queue, clCreateCommandQueue(device->context, device->device_id, 0, error));
    CL_ASSIGN_CHECK(buffer_mem->mem,   clCreateBuffer(device->context, CL_MEM_READ_WRITE, size, NULL, error));
    buffer_mem->write_event = NULL;
}

static void __device_init_2d_texture_storage(
    device_t* device,
    __device_texture_t* texture,
    cl_image_format* image_format,
    cl_image_desc* image_desc,
    int with_sample_image
) {
    #if defined(DEVICE_IMAGE_ENABLED) && !defined(DEVICE_RW_IMAGE_ENABLED)
        __device_init_2d_texture_buffer(device, &texture->mem, image_format, image_desc);
        if (with_sample_image)
        {
            __device_init_2d_image(device, &texture->sample_image, CL_MEM_READ_ONLY, image_format, image_desc, NULL);
        }
    #else
        // mem doubles as the sample source here, so with_sample_image is moot.
        (void) with_sample_image;
        __device_init_2d_image(device, &texture->mem, CL_MEM_READ_WRITE, image_format, image_desc, NULL);
    #endif
}

static cl_mem __device_acquire_mem(
    __device_mem_t* mem,
    cl_command_queue command_queue 
) {
    if (mem->write_event) 
    {
        CL_CHECK(clEnqueueBarrierWithWaitList(command_queue, 1, &mem->write_event, NULL));
    }

    return mem->mem;
}

static void __device_copy_mem(
    __device_mem_t* src,
    __device_mem_t* dst,
    size_t src_offset,
    size_t dst_offset,
    size_t size
) {
    __device_acquire_mem(src, dst->queue);

    if (dst->write_event)
    {
        CL_CHECK(clReleaseEvent(dst->write_event));
    }
    
    CL_CHECK(clEnqueueCopyBuffer(dst->queue, src->mem, dst->mem, src_offset, dst_offset, size, 0, NULL, &dst->write_event));
}

static void __device_read_mem(
    __device_mem_t* mem,
    cl_bool blocking_read,
    size_t offset,
    size_t size,
    void* data
) {
    CL_CHECK(clEnqueueReadBuffer(
        mem->queue,
        mem->mem, 
        blocking_read, 
        offset, 
        size, 
        data, 
        0, 
        NULL, 
        NULL
    ));
}

static device_event_t __device_write_mem(
    __device_mem_t* mem,
    cl_bool blocking_write,
    size_t offset,
    size_t size,
    const void* data
) {
    if (mem->write_event)
    {
        CL_CHECK(clReleaseEvent(mem->write_event));
        mem->write_event = NULL;
    }

    cl_event* wait_event = blocking_write ? NULL : &mem->write_event;

    CL_CHECK(clEnqueueWriteBuffer(
        mem->queue,
        mem->mem,
        blocking_write,
        offset,
        size,
        data,
        0,
        NULL,
        wait_event
    ));

    if (blocking_write) return NULL;

    CL_CHECK(clRetainEvent(mem->write_event));
    return mem->write_event;
}

static void __device_reset_mem(__device_mem_t* mem, size_t size, const void* value)
{
    device_event_t event = __device_write_mem(mem, CL_FALSE, 0, size, value);
    if (event) CL_CHECK(clReleaseEvent(event));
}

// TODO: Support more formats
static void __device_write_2d_texture(
    __device_mem_t* mem,
    cl_bool blocking_write,
    size_t origin[3],
    size_t region[3],
    size_t input_row_pitch,
    size_t input_slice_pitch,
    const void* data
) {
    if (mem->write_event) 
    {
        CL_CHECK(clReleaseEvent(mem->write_event));
    }

    cl_event* wait_event = blocking_write ? NULL : &mem->write_event;

    #ifdef DEVICE_IMAGE_ENABLED
    {
        CL_CHECK(clEnqueueWriteImage(
            mem->queue,
            mem->mem,
            blocking_write,
            origin,
            region,
            input_row_pitch,
            input_slice_pitch,
            data,
            0,
            NULL,
            wait_event
        ));
    }
    #else
    {
        size_t host_origin[3] = {0, 0, 0};
        size_t host_row_pitch = 0;
        size_t host_slice_pitch = 0;
        /*
        CL_CHECK(clEnqueueWriteBufferRect(
            mem->queue,
            mem->mem,
            blocking_write,
            origin,
            host_origin,
            region,
            input_row_pitch,
            input_slice_pitch,
            host_row_pitch,
            host_slice_pitch,
            data,
            0,
            NULL,
            wait_event
        ));
        */
        CL_CHECK(clEnqueueWriteBuffer(
            mem->queue,
            mem->mem,
            blocking_write,
            0,
            region[0]*region[1]*sizeof(cl_uint),
            data,
            0,
            NULL,
            wait_event
        ));
    }
    #endif
}

static void __device_barrier_mem(
    __device_mem_t* mem,
    cl_event event
) {
    CL_CHECK(clEnqueueBarrierWithWaitList(mem->queue, 1, &event, NULL));
}

static void __device_vertex_stage_wait(device_context_t* context, cl_command_queue queue)
{
    if (context->vertex_stage_event)
    {
        CL_CHECK(clEnqueueBarrierWithWaitList(queue, 1, &context->vertex_stage_event, NULL));
    }
}

static void __device_vertex_stage_publish(device_context_t* context, cl_event event)
{
    if (context->vertex_stage_event)
    {
        CL_CHECK(clReleaseEvent(context->vertex_stage_event));
    }
    CL_CHECK(clRetainEvent(event));
    context->vertex_stage_event = event;
}



// ---------------------------------------------------------------
// TODO: rm control from device.h to orchestrator.h
static size_t __device_get_vertex_command_index(device_context_t *context) 
{
    return context->vertex_command_queue_index % DEVICE_VERTEX_COMMAND_QUEUE_SIZE;
}

static cl_command_queue __device_get_vertex_command_queue(device_context_t *context) 
{
    return context->vertex_command_queues[__device_get_vertex_command_index(context)];
}

static void __device_advance_vertex_command_index(device_context_t *context) 
{
    context->vertex_command_queue_index += 1;
}

static void __device_reset_vertex_command_index(device_context_t *context) 
{
    context->vertex_command_queue_index = 0;
}
// ---------------------------------------------------------------

// ---------------------------------------------------------------
// Utils
static size_t __device_get_max_number_subtriangles() 
{
    return DEVICE_MAX_NUMBER_TRIANGLES + DEVICE_MAX_NUMBER_SUBTRIANGLES;
}

static size_t __device_get_max_number_triangles() 
{
    return DEVICE_MAX_NUMBER_TRIANGLES;
}

static size_t __device_get_max_number_bin_segments() 
{
    return CR_MAXBINS_SQR*16; // At least one segment x bin
}

static size_t __device_get_max_number_tile_segments() 
{
    return CR_MAXTILES_SQR*4; // At least one segment x tile
}

static size_t __device_get_bin_batch_size() 
{
    return DEVICE_BIN_SUB_GROUPS*DEVICE_SUB_GROUP_THREADS; // at least use all threads available, maybe can be fine tuned for larger executions
}

static size_t __device_get_num_bins_from_viewport(size_t viewport_size) 
{
    return ((viewport_size-1) / (CR_TILE_SIZE * CR_BIN_SIZE)) + 1;
}

static size_t __device_get_num_tiles_from_viewport(size_t viewport_size) 
{
    return ((viewport_size-1) / CR_TILE_SIZE) + 1;
}

static cl_mem __device_get_texture_vertex_buffer(device_context_t* context) 
{
    #ifdef DEVICE_IMAGE_ENABLED
        return context->t_vertex_buffer;
    #else
        return context->g_vertex_buffer;
    #endif
}

static cl_mem __device_get_texture_triangle_data(device_context_t* context) 
{
    #ifdef DEVICE_IMAGE_ENABLED
        return context->t_tri_data;
    #else
        return context->g_tri_data;
    #endif
}

static __device_bin_queue_t* __device_get_bin_queue(
    device_t* device,
    size_t bin_queue_id
) {
    return &device->bin_queues[bin_queue_id];
}

static cl_mem __device_get_texture_rt_mem(device_t* device, size_t texture_id)
{
    return device->textures[texture_id].mem.mem;
}

static __device_mem_t* __device_get_texture_sample_obj(device_t* device, size_t texture_id)
{
    #if defined(DEVICE_IMAGE_ENABLED) && !defined(DEVICE_RW_IMAGE_ENABLED)
        return &device->textures[texture_id].sample_image;
    #else
        return &device->textures[texture_id].mem;
    #endif
}


static uint32_t __device_get_size_from_name_type(const char* name_type) {
    #define RETURN_IF_SIZE_FROM(_TYPE)                              \
        if (strncmp(name_type, _TYPE, sizeof(_TYPE) - 1) == 0) {    \
            substr_size = name_type + sizeof(_TYPE) - 1;            \
            if (*substr_size == '*' || *substr_size == '\0') return 1;                     \
            return atoi(substr_size);                               \
        }

    const char* substr_size;
    RETURN_IF_SIZE_FROM("float");
    RETURN_IF_SIZE_FROM("int");
    RETURN_IF_SIZE_FROM("uint");
    RETURN_IF_SIZE_FROM("short");
    RETURN_IF_SIZE_FROM("char");
    RETURN_IF_SIZE_FROM("bool");

    #undef RETURN_IF_SIZE_FROM

    // OpenGL - OpenCL special types
    if (strcmp(name_type, "sampler2D_t") == 0) return 1;
    #ifdef HOSTGPU
    if (strcmp(name_type, "image_t") == 0) return 1;
    #else
    if (strncmp(name_type, "uchar", sizeof("uchar") -1) == 0) return 1;
    #endif

    printf("ERROR not found size for type_name=%s\n", name_type);
}

typedef enum {
    DEVICE_ARG_TYPE_ERROR,
    DEVICE_ARG_TYPE_BYTE = 0x1400,
    DEVICE_ARG_TYPE_SHORT = 0x1402,
    DEVICE_ARG_TYPE_INT = 0x1404,
    DEVICE_ARG_TYPE_FLOAT = 0x1406,
} device_arg_type_t;

static device_arg_type_t __device_get_arg_type_from_name_type(const char* name_type) 
{
    if (strncmp(name_type, "float",  sizeof("float") -1)  == 0) return DEVICE_ARG_TYPE_FLOAT;
    if (strncmp(name_type, "int",    sizeof("int")   -1)  == 0) return DEVICE_ARG_TYPE_INT;
    if (strncmp(name_type, "uint",    sizeof("uint")   -1)  == 0) return DEVICE_ARG_TYPE_INT;
    if (strncmp(name_type, "short",  sizeof("short") -1)  == 0) return DEVICE_ARG_TYPE_SHORT;
    if (strncmp(name_type, "char",   sizeof("char")  -1)  == 0) return DEVICE_ARG_TYPE_BYTE;
    if (strncmp(name_type, "bool",   sizeof("bool")  -1)  == 0) return DEVICE_ARG_TYPE_BYTE;
    
    printf("ERROR: not found type for type name=%s\n", name_type);

    #ifndef NDEBUG
    // printf("%s\n", name_type);
    // DEVICE_UNSUPORTED_MAPING();
    return DEVICE_ARG_TYPE_ERROR;
    #else
    return DEVICE_ARG_TYPE_ERROR;
    #endif
}

static int __device_uniform_in_stage(cl_kernel kernel, const char* name)
{
    cl_uint num_args;
    CL_CHECK(clGetKernelInfo(kernel, CL_KERNEL_NUM_ARGS, sizeof(cl_uint), &num_args, NULL));

    char arg_name[128];
    for (cl_uint a = 0; a + 1 < num_args; ++a)
    {
        CL_CHECK(clGetKernelArgInfo(kernel, a, CL_KERNEL_ARG_NAME, sizeof(arg_name), &arg_name, NULL));
        if (strcmp(arg_name, name) == 0) return 1;
    }
    return 0;
}

static void __device_set_framebuffer_data(
    gl_framebuffer_data_t* fb_data,
    size_t colorbuffer_id, 
    size_t depthbuffer_id, 
    size_t stencilbuffer_id
) {
    *fb_data = (gl_framebuffer_data_t){0};

    if (colorbuffer_id) 
    {
        set_framebuffer_data_colorbuffer_enabled(fb_data);
    } 
    else 
    {
        set_framebuffer_data_colorbuffer_disabled(fb_data);
    }
    if (depthbuffer_id) 
    {
        set_framebuffer_data_depthbuffer_enabled(fb_data);
    } 
    else 
    {
        set_framebuffer_data_depthbuffer_disabled(fb_data);
    }
    if (stencilbuffer_id) 
    {
        set_framebuffer_data_stencilbuffer_enabled(fb_data);
    } 
    else 
    {
        set_framebuffer_data_stencilbuffer_disabled(fb_data);
    }
}

// ---------------------------------------------------------------

// used for resetting memory atomic values
static const cl_int   ZERO                  = 0;
static const cl_uint  MAX_NUMBER_TRIANGLES  = DEVICE_MAX_NUMBER_TRIANGLES;

// ---------------------------------------------------------------

// Addressing/filter modes for device->samplers, indexed by the values returned
// from __device_sampler_address_index / __device_sampler_filter_index.
static const cl_addressing_mode __device_sampler_address_modes[3] = {
    CL_ADDRESS_CLAMP_TO_EDGE,
    CL_ADDRESS_REPEAT,
    CL_ADDRESS_MIRRORED_REPEAT,
};

static const cl_filter_mode __device_sampler_filter_modes[2] = {
    CL_FILTER_NEAREST,
    CL_FILTER_LINEAR,
};

static cl_uint __device_sampler_address_index(texture_data_t texture_data)
{
    cl_uint wrap_s = get_texture_data_wrap_s(texture_data);
    cl_uint wrap_t = get_texture_data_wrap_t(texture_data);

    if (wrap_s == TEXTURE_WRAP_CLAMP_TO_EDGE && wrap_t == TEXTURE_WRAP_CLAMP_TO_EDGE)
        return 0;

    if (wrap_s == TEXTURE_WRAP_MIRRORED_REPEAT || wrap_t == TEXTURE_WRAP_MIRRORED_REPEAT)
        return 2;

    return 1;
}

static cl_uint __device_sampler_filter_index(texture_data_t texture_data)
{
    if (get_texture_data_require_software_support(texture_data))
        return 0;

    return is_texture_data_linear(texture_data) ? 1 : 0;
}

#ifdef DEVICE_IMAGE_ENABLED
static cl_sampler __device_select_sampler(device_t* device, texture_data_t texture_data)
{
    return device->samplers
        [__device_sampler_address_index(texture_data)]
        [__device_sampler_filter_index(texture_data)];
}
#endif

// ---------------------------------------------------------------
// Device initializers
void device_init(device_t* shared)
{
    // OpenCL setup
    const cl_uint num_platforms = DEVICE_PLATFORM_ID + 1; 
    cl_platform_id platforms[num_platforms];
    CL_CHECK(clGetPlatformIDs(num_platforms, platforms, NULL));
    shared->platform_id = platforms[DEVICE_PLATFORM_ID];

    const cl_uint num_devices = DEVICE_DEVICE_ID + 1;
    cl_device_id devices[num_devices];
    CL_CHECK(clGetDeviceIDs(shared->platform_id, CL_DEVICE_TYPE_ALL, num_devices, devices, NULL));
    shared->device_id = devices[DEVICE_DEVICE_ID];

    CL_ASSIGN_CHECK(shared->context, clCreateContext(NULL, 1, &shared->device_id, NULL, NULL,  error));

    #ifdef DEVICE_IMAGE_ENABLED
    for (cl_uint address = 0; address < 3; ++address)
    {
        for (cl_uint filter = 0; filter < 2; ++filter)
        {
            CL_ASSIGN_CHECK(shared->samplers[address][filter],
                clCreateSampler(shared->context, CL_TRUE,
                    __device_sampler_address_modes[address],
                    __device_sampler_filter_modes[filter], error));
        }
    }
    #endif

    // Load programs
    size_t triangle_setup_size = triangle_setup_o_len;
    const unsigned char *triangle_setup_bin = triangle_setup_o;
    CL_ASSIGN_CHECK(shared->triangle_setup_program, clCreateProgramWithBinary(shared->context, 1, &shared->device_id, &triangle_setup_size, &triangle_setup_bin, NULL, error));
    CL_CHECK(clBuildProgram(shared->triangle_setup_program, 1, &shared->device_id, NULL, NULL, NULL));

    size_t bin_raster_size = bin_raster_o_len;
    const unsigned char *bin_raster_bin = bin_raster_o;
    CL_ASSIGN_CHECK(shared->bin_raster_program, clCreateProgramWithBinary(shared->context, 1, &shared->device_id, &bin_raster_size, &bin_raster_bin, NULL, error));
    CL_CHECK(clBuildProgram(shared->bin_raster_program, 1, &shared->device_id, NULL, NULL, NULL));

    size_t coarse_raster_size = coarse_raster_o_len;
    const unsigned char *coarse_raster_bin = coarse_raster_o;
    CL_ASSIGN_CHECK(shared->coarse_raster_program, clCreateProgramWithBinary(shared->context, 1, &shared->device_id, &coarse_raster_size, &coarse_raster_bin, NULL, error));
    CL_CHECK(clBuildProgram(shared->coarse_raster_program, 1, &shared->device_id, NULL, NULL, NULL));

    size_t force_clear_size = force_clear_o_len;
    const unsigned char *force_clear_bin = force_clear_o;
    CL_ASSIGN_CHECK(shared->clear_program, clCreateProgramWithBinary(shared->context, 1, &shared->device_id, &force_clear_size, &force_clear_bin, NULL, error));
    CL_CHECK(clBuildProgram(shared->clear_program, 1, &shared->device_id, NULL, NULL, NULL));

    size_t read_pixels_size = readnpixels_o_len;
    const unsigned char *read_pixels_bin = readnpixels_o;
    CL_ASSIGN_CHECK(shared->read_pixels_program, clCreateProgramWithBinary(shared->context, 1, &shared->device_id, &read_pixels_size, &read_pixels_bin, NULL, error));
    CL_CHECK(clBuildProgram(shared->read_pixels_program, 1, &shared->device_id, NULL, NULL, NULL));

    // Create kernels
    CL_ASSIGN_CHECK(shared->triangle_setup_arrays_kernel,       clCreateKernel(shared->triangle_setup_program,  "triangle_setup_arrays",        error));
    CL_ASSIGN_CHECK(shared->triangle_setup_arrays_block_kernel, clCreateKernel(shared->triangle_setup_program,  "triangle_setup_arrays_block",  error));
    CL_ASSIGN_CHECK(shared->triangle_setup_range_kernel,    clCreateKernel(shared->triangle_setup_program,  "triangle_setup_range",     error));
    CL_ASSIGN_CHECK(shared->bin_raster_kernel,              clCreateKernel(shared->bin_raster_program,      "bin_raster",               error));
    CL_ASSIGN_CHECK(shared->coarse_raster_kernel,           clCreateKernel(shared->coarse_raster_program,   "coarse_raster",            error));
    CL_ASSIGN_CHECK(shared->clear_kernel,                   clCreateKernel(shared->clear_program,           "force_clear",              error));
    CL_ASSIGN_CHECK(shared->read_pixels_kernel,             clCreateKernel(shared->read_pixels_program,     "readnpixels",              error));

    // Mem objects
    shared->textures_size = 1;
    shared->buffers_size = 1;

    cl_image_format image_format = {
        .image_channel_order = CL_RGBA,
        .image_channel_data_type = CL_UNSIGNED_INT32,
    };
    cl_image_desc image_desc = {
        .image_type = CL_MEM_OBJECT_IMAGE2D,
        .image_width = 1,
        .image_height = 1,
        .image_depth = 0,
        .image_array_size = 0,
        .image_row_pitch = 0,
        .image_slice_pitch = 0,
        .num_mip_levels = 0,
        .num_samples = 0,
        .buffer = NULL
    };
    __device_init_2d_texture_storage(shared, &shared->textures[0], &image_format, &image_desc, /*with_sample_image=*/1);

    __device_init_mem(shared, &shared->buffers[0], CL_MEM_READ_WRITE, sizeof(cl_uint), NULL);

    shared->bin_queues_size = 0;
    shared->programs_size = 0;
}

void device_init_context(
    device_context_t *context, 
    device_t* device
) {
    context->device = device;

    cl_uint max_number_bin_segments    = __device_get_max_number_bin_segments();
    cl_uint max_number_subtriangles    = __device_get_max_number_subtriangles();
    cl_uint max_number_tile_segments   = __device_get_max_number_tile_segments();
    cl_uint max_number_triangles       = __device_get_max_number_triangles();

    // Buffers
    const size_t triangle_header_size       = sizeof(triangle_header_t[max_number_subtriangles]);
    const size_t triangle_data_size         = sizeof(triangle_data_t[max_number_subtriangles]);
    const size_t vertex_buffer_size         = sizeof(cl_float4[DEVICE_VERTICES_SIZE][DEVICE_VARYING_SIZE + 1]);

    CL_ASSIGN_CHECK(context->g_tri_subtris,     clCreateBuffer(device->context, CL_MEM_READ_WRITE, sizeof(cl_uchar[max_number_subtriangles]),   NULL, error));
    CL_ASSIGN_CHECK(context->g_tri_header,      clCreateBuffer(device->context, CL_MEM_READ_WRITE, triangle_header_size,                        NULL, error));
    CL_ASSIGN_CHECK(context->g_tri_data,        clCreateBuffer(device->context, CL_MEM_READ_WRITE, triangle_data_size,                          NULL, error));     
    CL_ASSIGN_CHECK(context->g_vertex_buffer,   clCreateBuffer(device->context, CL_MEM_READ_WRITE, vertex_buffer_size,                          NULL, error));

    CL_ASSIGN_CHECK(context->g_uniform_arena,      clCreateBuffer(device->context, CL_MEM_READ_ONLY, sizeof(cl_uchar[DEVICE_MAX_BATCH_DRAWS][DEVICE_UNIFORM_CAPACITY]), NULL, error));
    CL_ASSIGN_CHECK(context->g_attribute_data_arena, clCreateBuffer(device->context, CL_MEM_READ_ONLY, sizeof(vertex_attribute_data_t[DEVICE_MAX_BATCH_DRAWS][DEVICE_VERTEX_ATTRIBUTE_SIZE]), NULL, error));
    CL_ASSIGN_CHECK(context->g_vertex_attribute_arena, clCreateBuffer(device->context, CL_MEM_READ_ONLY, sizeof(cl_float4[DEVICE_MAX_BATCH_DRAWS][DEVICE_VERTEX_ATTRIBUTE_SIZE]), NULL, error));
    CL_ASSIGN_CHECK(context->g_draw_start,         clCreateBuffer(device->context, CL_MEM_READ_ONLY, sizeof(cl_uint[DEVICE_MAX_BATCH_DRAWS + 1]), NULL, error)); // +1 = block sentinel
    CL_ASSIGN_CHECK(context->g_config,             clCreateBuffer(device->context, CL_MEM_READ_ONLY, sizeof(vertex_config_t[DEVICE_MAX_BATCH_DRAWS]), NULL, error));
    CL_ASSIGN_CHECK(context->g_setup_arena,        clCreateBuffer(device->context, CL_MEM_READ_ONLY, sizeof(setup_draw_config_t[DEVICE_MAX_BATCH_DRAWS + 1]), NULL, error)); // +1 = block sentinel

    #ifdef DEVICE_IMAGE_ENABLED
    {
        cl_image_desc image_desc;
        cl_image_format image_format;
        
        image_format = (cl_image_format) {
            .image_channel_order = CL_RGBA,
            .image_channel_data_type = CL_UNSIGNED_INT32,
        };

        image_desc = (cl_image_desc) {
            .image_type = CL_MEM_OBJECT_IMAGE1D_BUFFER,
            .image_row_pitch = 0,
            .image_width = triangle_header_size / sizeof(cl_uint4),
            .buffer = context->g_tri_header, 
        };
        
        CL_ASSIGN_CHECK(context->t_tri_header, clCreateImage(device->context, CL_MEM_READ_ONLY, &image_format, &image_desc, NULL, error));

        image_desc = (cl_image_desc) {
            .image_type = CL_MEM_OBJECT_IMAGE1D_BUFFER,
            .image_width = triangle_data_size / sizeof(cl_uint4),
            .image_row_pitch = 0,
            .buffer = context->g_tri_data, 
        };

        CL_ASSIGN_CHECK(context->t_tri_data, clCreateImage(device->context, CL_MEM_READ_ONLY, &image_format, &image_desc, NULL, error));

        image_format = (cl_image_format) {
            .image_channel_order = CL_RGBA,
            .image_channel_data_type = CL_FLOAT,
        };
        image_desc = (cl_image_desc) {
            .image_type = CL_MEM_OBJECT_IMAGE1D_BUFFER,
            .image_width = vertex_buffer_size / sizeof(cl_float4),
            .image_row_pitch = 0,
            .buffer = context->g_vertex_buffer,
        };

        CL_ASSIGN_CHECK(context->t_vertex_buffer, clCreateImage(device->context, CL_MEM_READ_WRITE, &image_format, &image_desc, NULL, error));
    }
    #endif

    CL_ASSIGN_CHECK(context->g_bin_first_seg,   clCreateBuffer(device->context, CL_MEM_READ_WRITE, sizeof(cl_int[CR_MAXBINS_SQR * CR_BIN_STREAMS_SIZE]), NULL, error)); 
    CL_ASSIGN_CHECK(context->g_bin_seg_data,    clCreateBuffer(device->context, CL_MEM_READ_WRITE, sizeof(cl_int[max_number_bin_segments * CR_BIN_SEG_SIZE]), NULL, error)); 
    CL_ASSIGN_CHECK(context->g_bin_seg_next,    clCreateBuffer(device->context, CL_MEM_READ_WRITE, sizeof(cl_int[max_number_bin_segments]), NULL, error));
    CL_ASSIGN_CHECK(context->g_bin_seg_count,   clCreateBuffer(device->context, CL_MEM_READ_WRITE, sizeof(cl_int[max_number_bin_segments]), NULL, error));
    CL_ASSIGN_CHECK(context->g_bin_total,       clCreateBuffer(device->context, CL_MEM_READ_WRITE, sizeof(cl_int[CR_MAXBINS_SQR * CR_BIN_STREAMS_SIZE]), NULL, error));

    CL_ASSIGN_CHECK(context->g_active_tiles,    clCreateBuffer(device->context, CL_MEM_READ_WRITE, sizeof(cl_int[CR_MAXTILES_SQR]), NULL, error));
    CL_ASSIGN_CHECK(context->g_tile_first_seg,  clCreateBuffer(device->context, CL_MEM_READ_WRITE, sizeof(cl_int[CR_MAXTILES_SQR]), NULL, error));
    CL_ASSIGN_CHECK(context->g_tile_seg_data,   clCreateBuffer(device->context, CL_MEM_READ_WRITE, sizeof(cl_int[max_number_tile_segments * CR_TILE_SEG_SIZE]), NULL, error));
    CL_ASSIGN_CHECK(context->g_tile_seg_next,   clCreateBuffer(device->context, CL_MEM_READ_WRITE, sizeof(cl_int[max_number_tile_segments]), NULL, error));
    CL_ASSIGN_CHECK(context->g_tile_seg_count,  clCreateBuffer(device->context, CL_MEM_READ_WRITE, sizeof(cl_int[max_number_tile_segments]), NULL, error));

    // Atomics
    cl_uint ZERO = 0;
    __device_init_mem(device, &context->a_bin_counter,      CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(cl_uint), &ZERO);
    __device_init_mem(device, &context->a_coarse_counter,   CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(cl_uint), &ZERO);
    __device_init_mem(device, &context->a_fine_counter,     CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(cl_uint), &ZERO);
    __device_init_mem(device, &context->a_num_active_tiles, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(cl_uint), &ZERO);
    __device_init_mem(device, &context->a_num_bin_segs,     CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(cl_uint), &ZERO);
    __device_init_mem(device, &context->a_num_subtris,      CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(cl_uint), &max_number_triangles);
    __device_init_mem(device, &context->a_num_tile_segs,    CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(cl_uint), &ZERO);

    // Vertex context objects
    context->vertex_command_queue_index = 0;
    context->vertex_stage_event = NULL;
    context->vertex_attribute_data_index = 0;
    context->vertex_uniform_index = 0;

    for (size_t i = 0; i < DEVICE_VERTEX_COMMAND_QUEUE_SIZE; ++i) {
        CL_ASSIGN_CHECK(context->vertex_command_queues[i],      clCreateCommandQueue(device->context, device->device_id, 0, error));

        __device_init_mem(device, &context->vertex_attribute_data_mem[i],   CL_MEM_READ_ONLY, sizeof(vertex_attribute_data_t[DEVICE_VERTEX_ATTRIBUTE_SIZE]), NULL);
        __device_init_mem(device, &context->vertex_uniform_mem[i],          CL_MEM_READ_ONLY, DEVICE_UNIFORM_CAPACITY, NULL);
        __device_init_mem(device, &context->index_buffer_cache[i],          CL_MEM_READ_ONLY, DEVICE_INDEX_BUFFER_CACHE_BYTES, NULL);
    }

    // Fragment context objects
    CL_ASSIGN_CHECK(context->raster_command_queue, clCreateCommandQueue(device->context, device->device_id, 0, error));

    __device_init_mem(context->device, &context->fragment_texture_datas_mem, CL_MEM_READ_ONLY, sizeof(texture_data_t[DEVICE_TEXTURE_UNITS]), NULL);

    __device_init_mem(context->device, &context->fragment_uniform_mem, CL_MEM_READ_ONLY, sizeof(cl_uchar[TRIANGLE_PRIMITIVE_CONFIGS][DEVICE_UNIFORM_CAPACITY]), NULL);

    __device_init_mem(context->device, &context->rop_configs_mem, CL_MEM_READ_ONLY, sizeof(rop_config_t[TRIANGLE_PRIMITIVE_CONFIGS]), NULL);    

    // Default bind state
    for (size_t attribute = 0; attribute < DEVICE_VERTEX_ATTRIBUTE_SIZE; ++attribute)
    {
        context->vertex_attribute_pointers[attribute].is_host = 0;
        context->vertex_attribute_pointers[attribute].mem.device_id = 0;

        context->host_attr_snapshot[attribute].mem = NULL;
        context->host_attr_snapshot[attribute].write_event = NULL;
        CL_ASSIGN_CHECK(context->host_attr_snapshot[attribute].queue, clCreateCommandQueue(device->context, device->device_id, 0, error));
        context->host_attr_snapshot_cap[attribute] = 0;
    }

    for (size_t texture_unit = 0; texture_unit < DEVICE_TEXTURE_UNITS; ++texture_unit)
    {
        context->texture_units_ids[texture_unit] = 0;
        #ifdef DEVICE_IMAGE_ENABLED
        context->fragment_texture_samplers[texture_unit] = device->samplers[0][0];
        #endif
    }

}
// ---------------------------------------------------------------

// ---------------------------------------------------------------
// Kernel argument setters.

static void __device_set_vertex_arena_args(
    cl_kernel kernel, cl_uint* arg_idx,
    size_t num_attributes,
    cl_mem g_vertex_attribute,
    cl_mem g_attribute_data_arena,
    cl_mem g_uniform_arena,
    cl_mem attribute_pointers[DEVICE_VERTEX_ATTRIBUTE_SIZE]
) {
    CL_CHECK(clSetKernelArg(kernel, (*arg_idx)++, sizeof(cl_mem), &g_vertex_attribute));
    CL_CHECK(clSetKernelArg(kernel, (*arg_idx)++, sizeof(cl_mem), &g_attribute_data_arena));
    CL_CHECK(clSetKernelArg(kernel, (*arg_idx)++, sizeof(cl_mem), &g_uniform_arena));
    for (cl_uint attribute = 0; attribute < num_attributes; ++attribute)
        CL_CHECK(clSetKernelArg(kernel, (*arg_idx)++, sizeof(cl_mem), &attribute_pointers[attribute]));
}

static void __device_set_vertex_shader_direct_args(
    cl_kernel kernel,
    device_context_t* context,
    size_t num_attributes,
    cl_mem g_vertex_attribute,
    cl_mem g_attribute_data_arena,
    cl_mem g_uniform_arena,
    cl_mem attribute_pointers[DEVICE_VERTEX_ATTRIBUTE_SIZE],
    cl_uint c_num_vertices,
    cl_uint c_vertex_offset,
    vertex_config_t c_config
) {
    cl_mem t_vertex_buffer = __device_get_texture_vertex_buffer(context);
    cl_uint arg_idx = 0;

    __device_set_vertex_arena_args(kernel, &arg_idx, num_attributes, g_vertex_attribute, g_attribute_data_arena, g_uniform_arena, attribute_pointers);

    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem),          &t_vertex_buffer));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_num_vertices),  &c_num_vertices));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_vertex_offset), &c_vertex_offset));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_config),        &c_config));
}

// BLOCK: arenas + gl_draw_start (search) + per-draw config array.
static void __device_set_vertex_shader_block_args(
    cl_kernel kernel,
    device_context_t* context,
    size_t num_attributes,
    cl_mem g_vertex_attribute,
    cl_mem g_attribute_data_arena,
    cl_mem g_uniform_arena,
    cl_mem g_draw_start,
    cl_mem g_config,
    cl_mem attribute_pointers[DEVICE_VERTEX_ATTRIBUTE_SIZE],
    cl_uint c_num_vertices,
    cl_uint c_first_draw,
    cl_uint c_num_draws,
    cl_uint c_vertex_offset
) {
    cl_mem t_vertex_buffer = __device_get_texture_vertex_buffer(context);
    cl_uint arg_idx = 0;

    // Block ABI order: arenas, draw_start, configs, attribute pointers, vertex_buffer, scalars.
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &g_vertex_attribute));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &g_attribute_data_arena));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &g_uniform_arena));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &g_draw_start));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &g_config));
    for (cl_uint attribute = 0; attribute < num_attributes; ++attribute)
        CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &attribute_pointers[attribute]));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem),          &t_vertex_buffer));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_num_vertices),  &c_num_vertices));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_first_draw),    &c_first_draw));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_num_draws),     &c_num_draws));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_vertex_offset), &c_vertex_offset));
}

static void __device_set_triangle_setup_arrays_kernel_args(
    cl_kernel kernel,
    device_context_t* context, 
    cl_uint c_num_tris,
    cl_uint c_vertex_offset,
    render_mode_t c_mode,
    cl_uint c_vertex_size,
    cl_uint c_fragment_config_id,
    cl_uint c_viewport_height,
    cl_uint c_viewport_width
) {
    cl_uint c_max_subtris = __device_get_max_number_subtriangles();
    cl_uint c_samples_log2 = 0; // TODO: not impl
    cl_mem  t_vertex_buffer = __device_get_texture_vertex_buffer(context);

    cl_uint arg_idx = 0;

    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem),                 &context->a_num_subtris));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem),                 &context->g_tri_header));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem),                 &context->g_tri_data));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem),                 &context->g_tri_subtris));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem),                 &t_vertex_buffer));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_num_tris),             &c_num_tris));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_vertex_offset),        &c_vertex_offset));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_max_subtris),          &c_max_subtris));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_mode),                 &c_mode));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_samples_log2),         &c_samples_log2));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_vertex_size),          &c_vertex_size));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_viewport_height),      &c_viewport_height));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_viewport_width),       &c_viewport_width));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_fragment_config_id),   &c_fragment_config_id));}

static void __device_set_triangle_setup_range_kernel_args(
    cl_kernel kernel,
    device_context_t* context,
    cl_mem g_index_buffer,
    cl_uint c_num_tris,
    cl_uint c_vertex_offset,
    render_mode_t c_mode,
    cl_uint c_vertex_size,
    cl_uint c_fragment_config_id,
    cl_uint c_viewport_height,
    cl_uint c_viewport_width
) {
    cl_uint c_max_subtris = __device_get_max_number_subtriangles();
    cl_uint c_samples_log2 = 0; // TODO: not impl
    cl_mem  t_vertex_buffer = __device_get_texture_vertex_buffer(context);

    cl_uint arg_idx = 0;

    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem),                 &context->a_num_subtris));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem),                 &g_index_buffer));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem),                 &context->g_tri_header));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem),                 &context->g_tri_data));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem),                 &context->g_tri_subtris));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem),                 &t_vertex_buffer)); 
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_num_tris),             &c_num_tris));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_vertex_offset),        &c_vertex_offset));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_max_subtris),          &c_max_subtris));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_mode),                 &c_mode));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_samples_log2),         &c_samples_log2));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_vertex_size),          &c_vertex_size));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_viewport_height),      &c_viewport_height));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_viewport_width),       &c_viewport_width));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_fragment_config_id),   &c_fragment_config_id));}

static void __device_set_bin_raster_kernel_args(
    cl_kernel kernel,
    device_context_t* context,
    cl_uint c_num_tris,
    cl_uint c_viewport_height,
    cl_uint c_viewport_width
) {
    cl_uint c_max_subtris = __device_get_max_number_subtriangles();
    cl_uint c_max_bin_segs  = __device_get_max_number_bin_segments();
    cl_uint c_bin_batch_sz = __device_get_bin_batch_size();
    cl_uint c_height_bins = __device_get_num_bins_from_viewport(c_viewport_height);
    cl_uint c_width_bins = __device_get_num_bins_from_viewport(c_viewport_width);
    cl_uint c_num_bins = c_height_bins * c_width_bins;

    cl_uint arg_idx = 0;

    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &context->a_bin_counter));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &context->a_num_bin_segs));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &context->a_num_subtris));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &context->g_bin_first_seg));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &context->g_bin_seg_count));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &context->g_bin_seg_data));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &context->g_bin_seg_next));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &context->g_bin_total));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &context->g_tri_header));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &context->g_tri_subtris));
    #ifdef DEVICE_IMAGE_ENABLED
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &context->t_tri_header));
    #endif
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_bin_batch_sz),      &c_bin_batch_sz));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_height_bins),       &c_height_bins));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_max_bin_segs),      &c_max_bin_segs));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_max_subtris),       &c_max_subtris));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_num_bins),          &c_num_bins));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_num_tris),          &c_num_tris));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_viewport_height),   &c_viewport_height));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_viewport_width),    &c_viewport_width));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_width_bins),        &c_width_bins));
}

static void __device_set_coarse_raster_kernel_args(
    cl_kernel kernel,
    device_context_t* context,
    cl_uint c_deferred_clear,
    cl_uint c_viewport_height,
    cl_uint c_viewport_width
) {
    cl_uint c_max_subtris = __device_get_max_number_subtriangles();
    cl_uint c_max_bin_segs  = __device_get_max_number_bin_segments();
    cl_uint c_max_tile_segs = __device_get_max_number_tile_segments(); // At least one segment x tile
    cl_uint c_height_tiles = __device_get_num_tiles_from_viewport(c_viewport_height);
    cl_uint c_width_tiles = __device_get_num_tiles_from_viewport(c_viewport_width);

    cl_uint c_height_bins = __device_get_num_bins_from_viewport(c_viewport_height);
    cl_uint c_width_bins = __device_get_num_bins_from_viewport(c_viewport_width);
    cl_uint c_num_bins = c_height_bins * c_width_bins;

    cl_uint arg_idx = 0;

    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem),              &context->a_coarse_counter));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem),              &context->a_num_active_tiles));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem),              &context->a_num_bin_segs));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem),              &context->a_num_tile_segs));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem),              &context->a_num_subtris));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem),              &context->g_active_tiles));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem),              &context->g_bin_first_seg));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem),              &context->g_bin_seg_count));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem),              &context->g_bin_seg_data));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem),              &context->g_bin_seg_next));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem),              &context->g_bin_total));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem),              &context->g_tile_first_seg));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem),              &context->g_tile_seg_count));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem),              &context->g_tile_seg_data));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem),              &context->g_tile_seg_next));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem),              &context->g_tri_header));
    #ifdef DEVICE_IMAGE_ENABLED
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem),              &context->t_tri_header));
    #endif
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_deferred_clear),    &c_deferred_clear));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_height_tiles),      &c_height_tiles));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_max_bin_segs),      &c_max_bin_segs));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_max_subtris),       &c_max_subtris));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_max_tile_segs),     &c_max_tile_segs));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_num_bins),          &c_num_bins));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_viewport_height),   &c_viewport_height));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_viewport_width),    &c_viewport_width));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_width_bins),        &c_width_bins));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_width_tiles),       &c_width_tiles));}

static void __device_set_fragment_shader_kernel_args(
    cl_kernel kernel,
    device_context_t* context,
    cl_mem t_colorbuffer,
    cl_mem t_depthbuffer,
    cl_mem t_stencilbuffer,
    clear_data_t c_clear_data,
    enabled_data_t c_enabled_data,
    cl_uint c_colorbuffer_mode,
    cl_uint c_viewport_height,
    cl_uint c_viewport_width,
    gl_framebuffer_data_t c_framebuffer_data
) {
    cl_mem t_tri_data = __device_get_texture_triangle_data(context);
    cl_mem t_vertex_buffer = __device_get_texture_vertex_buffer(context);
    const cl_uint c_max_bin_segs  = __device_get_max_number_bin_segments();
    const cl_uint c_max_tile_segs = __device_get_max_number_tile_segments();
    const cl_uint c_max_subtris = __device_get_max_number_subtriangles();
    cl_uint c_width_tiles = __device_get_num_tiles_from_viewport(c_viewport_width);

    cl_uint arg_idx = 0;
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &context->fragment_uniform_mem));

    for(int i=0; i<DEVICE_TEXTURE_UNITS; ++i)
    {
        __device_mem_t *mem = __device_get_texture_sample_obj(context->device, context->texture_units_ids[i]);
        CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &mem->mem));
        #ifdef DEVICE_IMAGE_ENABLED
        CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_sampler), &context->fragment_texture_samplers[i]));
        #endif
    }

    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &context->fragment_texture_datas_mem));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &context->a_fine_counter));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &context->a_num_active_tiles));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &context->a_num_bin_segs));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &context->a_num_subtris));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &context->a_num_tile_segs));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &context->g_active_tiles));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &context->g_tile_first_seg));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &context->g_tile_seg_count));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &context->g_tile_seg_data));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &context->g_tile_seg_next));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &context->g_tri_header));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &t_colorbuffer));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &t_depthbuffer));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &t_stencilbuffer));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &t_tri_data));
    #ifdef DEVICE_IMAGE_ENABLED
    {
        CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &context->t_tri_header));
    }
    #endif
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem),              &t_vertex_buffer));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem),              &context->rop_configs_mem));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_clear_data),        &c_clear_data));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_enabled_data),      &c_enabled_data));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_colorbuffer_mode),  &c_colorbuffer_mode));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_max_bin_segs),      &c_max_bin_segs));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_max_subtris),       &c_max_subtris));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_max_tile_segs),     &c_max_tile_segs));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_viewport_height),   &c_viewport_height));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_viewport_width),    &c_viewport_width));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_width_tiles),       &c_width_tiles));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_framebuffer_data),  &c_framebuffer_data));
}

static void __device_set_clear_kernel_args(
    device_context_t* context,
    cl_kernel kernel,
    cl_mem t_color_buffer,
    cl_mem t_depth_buffer,
    cl_mem t_stencil_buffer,
    cl_uint c_colorbuffer_mode,
    cl_uint c_viewport_width,
    clear_data_t c_data,
    enabled_data_t c_enabled
) {
    cl_uint arg_idx = 0;

    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &t_color_buffer));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &t_depth_buffer));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem), &t_stencil_buffer));
    #ifndef DEVICE_IMAGE_ENABLED
    {
        CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_colorbuffer_mode), &c_colorbuffer_mode));
        CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_viewport_width), &c_viewport_width));
    }
    #endif
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_data),      &c_data));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_enabled),   &c_enabled));
}

static void __device_set_read_pixels_kernel_args(
    cl_kernel kernel,
    cl_mem t_colorbuffer,
    cl_mem g_buffer,
    cl_uint c_colorbuffer_mode,
    cl_uint c_buffer_mode,
    cl_uchar c_swap_rb,
    cl_uchar c_swap_y
) {
    cl_uint arg_idx = 0;

    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(t_colorbuffer), &t_colorbuffer));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(g_buffer), &g_buffer))
    // clSetKernelArgSVMPointer(kernel, arg_idx++, &g_buffer);
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_colorbuffer_mode), &c_colorbuffer_mode));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_buffer_mode), &c_buffer_mode));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_swap_rb), &c_swap_rb));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_swap_y), &c_swap_y));
}
// ---------------------------------------------------------------

void device_finish(device_context_t* context) 
{
    CL_CHECK(clFinish(context->raster_command_queue));
}

// ---------------------------------------------------------------
// Device create functions

size_t device_create_bin_queue(
    device_t* device
) {
    size_t bin_queue_id = device->bin_queues_size;

    if (bin_queue_id >= HOST_BIN_QUEUES_SIZE) {
        fprintf(stderr, "Error: Exceeded maximum number of bin queues.\n");
        exit(1);
    }

    __device_bin_queue_t* bin_queue = &device->bin_queues[bin_queue_id];

    for(int i=0; i<DEVICE_BIN_QUEUE_SIZE; ++i)
    {
        for(int j=0; j<DEVICE_BIN_QUEUE_SIZE; ++j)
        {
            CL_ASSIGN_CHECK(bin_queue->queues[i][j], clCreateCommandQueue(
                device->context,
                device->device_id,
                0,
                error
            ));
        }
    }

    device->bin_queues_size += 1;

    return bin_queue_id;
}

size_t device_create_program_from_binary(device_t* device, size_t size, const unsigned char* binary) 
{
    size_t program_id = device->programs_size;

    // create program object
    CL_ASSIGN_CHECK(device->programs[program_id], clCreateProgramWithBinary(device->context, 1, &device->device_id, &size, &binary, NULL, error));
    CL_CHECK(clBuildProgram(device->programs[program_id], 1, &device->device_id, NULL, NULL, NULL));

    // create program kernels
    cl_program program = device->programs[program_id];
    __device_shader_kernels_t* kernels = &device->shaders[program_id];

    CL_ASSIGN_CHECK(kernels->vertex.kernel_direct,  clCreateKernel(program, "vertex_shader_direct_kernel",  error));
    CL_ASSIGN_CHECK(kernels->vertex.kernel_block,   clCreateKernel(program, "vertex_shader_block_kernel",   error));
    CL_ASSIGN_CHECK(kernels->fragment,              clCreateKernel(program, "fine_raster_single_sample",    error));
    CL_ASSIGN_CHECK(kernels->uniform_data,          clCreateKernel(program, "gl_uniform_data",              error));
    CL_ASSIGN_CHECK(kernels->vs_uniform_data,       clCreateKernel(program, "gl_vs_uniform_data",           error));
    CL_ASSIGN_CHECK(kernels->fs_uniform_data,       clCreateKernel(program, "gl_fs_uniform_data",           error));
    CL_ASSIGN_CHECK(kernels->attribute_data,        clCreateKernel(program, "gl_attribute_data",            error));
    CL_ASSIGN_CHECK(kernels->varying_data,          clCreateKernel(program, "gl_varying_data",              error));

    cl_uint kernel_varying_num_args;
    CL_CHECK(clGetKernelInfo(kernels->varying_data, CL_KERNEL_NUM_ARGS, sizeof(cl_uint), &kernel_varying_num_args, NULL));
    kernels->vertex.vertex_size = sizeof(cl_float4[kernel_varying_num_args]);

    cl_uint kernel_attributes_num_args;
    CL_CHECK(clGetKernelInfo(kernels->attribute_data, CL_KERNEL_NUM_ARGS, sizeof(cl_uint), &kernel_attributes_num_args, NULL));
    kernels->vertex.number_attributes = kernel_attributes_num_args - 1;

    device->programs_size += 1;

    return program_id;
}
// ---------------------------------------------------------------

// ---------------------------------------------------------------
// Device getter functions
void device_get_program_uniform_arg_data(device_t* shared, size_t program_id, size_t location, arg_data_t* arg_data) 
{
    __device_shader_kernels_t* kernels = &shared->shaders[program_id];
    cl_kernel kernel = kernels->uniform_data;

    char name[128];
    char type_name[32];

    CL_CHECK(clGetKernelArgInfo(kernel, location, CL_KERNEL_ARG_NAME,        sizeof(name),       &name,      NULL));
    CL_CHECK(clGetKernelArgInfo(kernel, location, CL_KERNEL_ARG_TYPE_NAME,   sizeof(type_name),  &type_name, NULL));

    arg_data->size = __device_get_size_from_name_type(type_name);
    arg_data->type = __device_get_arg_type_from_name_type(type_name);
    strcpy(arg_data->name, name);

    int in_vs = __device_uniform_in_stage(kernels->vs_uniform_data, name);
    int in_fs = __device_uniform_in_stage(kernels->fs_uniform_data, name);
    int untagged = !in_vs && !in_fs;

    arg_data->vertex_location   = (in_vs            ) ? (unsigned int) location : ARG_LOCATION_NONE;
    arg_data->fragment_location = (in_fs || untagged) ? (unsigned int) location : ARG_LOCATION_NONE;
}

size_t device_get_program_uniform_size(device_t* device, size_t program_id)
{
    cl_kernel kernel = device->shaders[program_id].uniform_data;

    cl_uint uniform_size;
    CL_CHECK(clGetKernelInfo(kernel, CL_KERNEL_NUM_ARGS, sizeof(cl_uint), &uniform_size, NULL));

    return uniform_size - 1;
}

void device_get_program_vertex_attrib_arg_data(device_t* device, size_t program_id, size_t location, arg_data_t* arg_data)
{
    cl_kernel kernel = device->shaders[program_id].attribute_data;

    char name[128];
    char type_name[32];

    CL_CHECK(clGetKernelArgInfo(kernel, location, CL_KERNEL_ARG_NAME,        sizeof(name),       &name,      NULL));
    CL_CHECK(clGetKernelArgInfo(kernel, location, CL_KERNEL_ARG_TYPE_NAME,   sizeof(type_name),  &type_name, NULL));

    arg_data->size = __device_get_size_from_name_type(type_name);
    arg_data->type = __device_get_arg_type_from_name_type(type_name);
    strcpy(arg_data->name, &name[1]);
}

size_t device_get_program_vertex_attrib_size(device_t* device, size_t program_id)
{
    return device->shaders[program_id].vertex.number_attributes;
}
// ---------------------------------------------------------------


static size_t __device_get_bytes_from_texture_mode(cl_uint texture_mode) 
{
    switch (texture_mode) {
        case TEX_R8:
        case TEX_STENCIL_INDEX8:
            return 1;
        case TEX_RG8:
        case TEX_RGBA4:
        case TEX_RGB5_A1:
        case TEX_RGB565:
        case TEX_DEPTH_COMPONENT16:
            return 2;
        case TEX_RGB8:
            return 3;
        default:
        case TEX_RGBA8:
            return 4;
    }
}

static cl_image_format __device_get_image_format_from_texture_mode(cl_uint texture_mode) 
{
    cl_image_format image_format;

    switch (texture_mode) {
        case TEX_R8:
            image_format = (cl_image_format) {
                .image_channel_order = CL_R,
                .image_channel_data_type = CL_UNORM_INT8,
            };
            break;
        case TEX_STENCIL_INDEX8:
            image_format = (cl_image_format) {
                .image_channel_order = CL_A,
                .image_channel_data_type = CL_UNORM_INT8,
            };
            break;
        case TEX_RG8:
            image_format = (cl_image_format) {
                .image_channel_order = CL_RG,
                .image_channel_data_type = CL_UNORM_INT8,
            };
            break;
        case TEX_RGB8:
            image_format = (cl_image_format) {
                .image_channel_order = CL_RGB,
                .image_channel_data_type = CL_UNORM_INT8,
            };
            break;
        case TEX_RGB565:
            image_format = (cl_image_format) {
                .image_channel_order = CL_RGB,
                .image_channel_data_type = CL_UNORM_SHORT_565,
            };
            break;
        case TEX_RGBA4:
            image_format = (cl_image_format) {
                .image_channel_order = CL_RG,
                .image_channel_data_type = CL_UNORM_INT8,
            };
            break;
        case TEX_RGB5_A1:
            image_format = (cl_image_format) {
                .image_channel_order = CL_R,
                .image_channel_data_type = CL_UNORM_INT16,
            };
            break;
        case TEX_DEPTH_COMPONENT16:
            image_format = (cl_image_format) {
                // TODO: this may be not supported for OpenCL 1.2 check cl_khr_depth_images extension
                .image_channel_order = CL_DEPTH,
                .image_channel_data_type = CL_UNORM_INT16,
            };
            break;
        default:
        case TEX_RGBA8:
            image_format = (cl_image_format) {
                .image_channel_order = CL_RGBA,
                .image_channel_data_type = CL_UNORM_INT8,
            };
            break;
    }

    return image_format;
}

size_t device_create_ro_buffer(device_t* device, size_t size) 
{
    size_t buffer_id = device->buffers_size;

    __device_init_mem(device, &device->buffers[buffer_id], CL_MEM_READ_ONLY, size, NULL);
    
    device->buffers_size += 1;

    return buffer_id;
}

static size_t __device_create_2d_texture(device_t* device, size_t width, size_t height, size_t levels, cl_uint texture_mode, int with_sample_image)
{
    size_t texture_id = device->textures_size;

    if (texture_id >= HOST_TEXTURES_SIZE) {
        fprintf(stderr, "Error: device texture storage exceeded HOST_TEXTURES_SIZE (%d); texture_id=%zu\n",
                HOST_TEXTURES_SIZE, texture_id);
        exit(1);
    }

    #ifdef DEVICE_IMAGE_ENABLED
    size_t store_levels = 1;
    #else
    size_t store_levels = levels;
    #endif

    cl_image_format image_format = __device_get_image_format_from_texture_mode(texture_mode);
    cl_image_desc image_desc = {
        .image_type = CL_MEM_OBJECT_IMAGE2D,
        .image_width = width,
        .image_height = height,
        .image_depth = 0,
        .image_array_size = 0,
        .image_row_pitch = 0,
        .image_slice_pitch = 0,
        .num_mip_levels = store_levels,
        .num_samples = 0,
        .buffer = NULL
    };

    __device_init_2d_texture_storage(device, &device->textures[texture_id], &image_format, &image_desc, with_sample_image);

    device->textures_size += 1;

    return texture_id;
}

size_t device_create_2d_texture(device_t* device, size_t width, size_t height, size_t levels, cl_uint texture_mode)
{
    return __device_create_2d_texture(device, width, height, levels, texture_mode, /*with_sample_image=*/1);
}

size_t device_create_renderbuffer(device_t* device, size_t width, size_t height, cl_uint texture_mode)
{
    return __device_create_2d_texture(device, width, height, 1, texture_mode, /*with_sample_image=*/0);
}

void device_generate_2d_mipmap(device_t* device, size_t texture_id, size_t width, size_t height, size_t levels, cl_uint texture_mode)
{
    if (levels <= 1) return;

    // It does generate mipmaps automatically when using images
    #ifndef DEVICE_IMAGE_ENABLED

    // TODO: implement mipmap generation using OpenCL kernels for devices that do not support images.
    size_t bpp = __device_get_bytes_from_texture_mode(texture_mode);
    int packed = (texture_mode == TEX_RGBA4 || texture_mode == TEX_RGB565 || texture_mode == TEX_RGB5_A1);

    // Total packed size across the mip chain.
    size_t total = 0, lw = width, lh = height;
    for (size_t l = 0; l < levels; ++l) { total += lw * lh; lw = lw > 1 ? lw/2 : 1; lh = lh > 1 ? lh/2 : 1; }
    total *= bpp;

    unsigned char* buf = (unsigned char*) malloc(total);
    __device_mem_t* mem = &device->textures[texture_id].mem;
    CL_CHECK(clEnqueueReadBuffer(mem->queue, mem->mem, CL_TRUE, 0, width*height*bpp, buf, 0, NULL, NULL));

    size_t src_off = 0, sw = width, sh = height;
    for (size_t l = 1; l < levels; ++l)
    {
        size_t dw = sw > 1 ? sw/2 : 1;
        size_t dh = sh > 1 ? sh/2 : 1;
        size_t dst_off = src_off + sw*sh*bpp;
        for (size_t y = 0; y < dh; ++y) for (size_t x = 0; x < dw; ++x)
        {
            unsigned char* dst = buf + (dst_off + (y*dw + x)*bpp);
            size_t x0 = x*2, y0 = y*2;
            size_t x1 = (x0+1 < sw) ? x0+1 : x0;
            size_t y1 = (y0+1 < sh) ? y0+1 : y0;
            const unsigned char* s00 = buf + (src_off + (y0*sw + x0)*bpp);
            if (packed) { memcpy(dst, s00, bpp); continue; }
            const unsigned char* s10 = buf + (src_off + (y0*sw + x1)*bpp);
            const unsigned char* s01 = buf + (src_off + (y1*sw + x0)*bpp);
            const unsigned char* s11 = buf + (src_off + (y1*sw + x1)*bpp);
            for (size_t c = 0; c < bpp; ++c)
                dst[c] = (unsigned char)(((unsigned)s00[c] + s10[c] + s01[c] + s11[c] + 2) / 4);
        }
        src_off = dst_off; sw = dw; sh = dh;
    }

    // Write back levels 1..N-1 (level 0 unchanged).
    CL_CHECK(clEnqueueWriteBuffer(mem->queue, mem->mem, CL_TRUE,
        width*height*bpp, total - width*height*bpp, buf + width*height*bpp, 0, NULL, NULL));
    free(buf);
    #endif // DEVICE_IMAGE_ENABLED
}


void device_destroy(device_t* device) NOT_IMPLEMENTED;

void device_destroy_context(device_context_t* context) NOT_IMPLEMENTED;

static void __device_print_vertex_shader_output(
    device_context_t* context,
    cl_command_queue queue,
    size_t num_vertices,
    size_t num_varying
) {
    // printf("enqueue of=%ld, sz=%ld\n",gw_offset, gw_size);
    size_t vertices_sizeof = sizeof(float[num_vertices][num_varying+1][4]);
    float *vertices = (float*) malloc(vertices_sizeof);
    clEnqueueReadBuffer(queue, context->g_vertex_buffer, CL_TRUE, 0, vertices_sizeof, vertices, 0, NULL, NULL);
    
    for(size_t i= 0; i<num_vertices; ++i) 
    {
        printf("vertex %ld:", i);
        for(size_t j=0; j<num_varying+1; ++j) {
            size_t offset = i*(num_varying+1)*4 + j*4;
            printf(" (%.2f,%.2f,%.2f,%.2f)", 
                vertices[offset+0],
                vertices[offset+1],
                vertices[offset+2],
                vertices[offset+3]
            );
        }
        printf("\n");
    }

    free(vertices);
}

static void __device_print_triangle_assembly_output(
    device_context_t* context,
    cl_command_queue queue,
    size_t triangles
) {
    size_t sizeof_primitives = sizeof(cl_uchar[triangles]);
    cl_uchar *primitives = (cl_uchar*) malloc(sizeof_primitives);
    CL_CHECK(clEnqueueReadBuffer(queue, context->g_tri_subtris, CL_TRUE, 0, sizeof_primitives, primitives, 0, NULL, NULL));

    printf("subtri=[%d", primitives[0]);
    for(size_t i= 1; i<triangles; ++i) {
        printf(",%d", primitives[i]);
    }
    printf("]\n");

    free(primitives);

    size_t sizeof_header = sizeof(triangle_header_t[triangles]);
    triangle_header_t *triangle_header = (triangle_header_t*) malloc(sizeof_header);
    CL_CHECK(clEnqueueReadBuffer(queue, context->g_tri_header, CL_TRUE, 0, sizeof_header, triangle_header, 0, NULL, NULL));

    for(size_t i=0; i<triangles; ++i) {
        triangle_header_t *header = &triangle_header[i];
        printf("header[%ld]={v0x=%d,v0y=%d,v1x=%d,v1y=%d,v2x=%d,v2y=%d}\n",i, 
            header->v0x >> (CR_SUBPIXEL_LOG2 - 1), 
            header->v0y >> (CR_SUBPIXEL_LOG2 - 1),
            header->v1x >> (CR_SUBPIXEL_LOG2 - 1),
            header->v1y >> (CR_SUBPIXEL_LOG2 - 1),
            header->v2x >> (CR_SUBPIXEL_LOG2 - 1), 
            header->v2y >> (CR_SUBPIXEL_LOG2 - 1)
        );
    }

    free(triangle_header);
}

//-------------------------------------------------------------------------------------

int device_snapshot_host_attributes(device_context_t* context, size_t shader_id, size_t num_vertices)
{
    size_t num_attributes = context->device->shaders[shader_id].vertex.number_attributes;
    int    any_host = 0;

    for (size_t attribute = 0; attribute < num_attributes; ++attribute)
    {
        __device_vertex_attribute_pointer_t* ap = &context->vertex_attribute_pointers[attribute];

        if (!ap->is_host || ap->mem.host == NULL) continue;

        size_t stride = ap->stride;
        size_t bytes  = num_vertices * stride;
        if (bytes == 0) continue;

        any_host = 1;

        __device_mem_t* snap = &context->host_attr_snapshot[attribute];

        if (bytes > context->host_attr_snapshot_cap[attribute])
        {
            if (snap->mem) CL_CHECK(clReleaseMemObject(snap->mem));
            CL_ASSIGN_CHECK(snap->mem, clCreateBuffer(context->device->context, CL_MEM_READ_ONLY, bytes, NULL, error));
            context->host_attr_snapshot_cap[attribute] = bytes;
        }

        __device_write_mem(snap, CL_TRUE, 0, bytes, ap->mem.host);
    }

    return any_host;
}

void device_launch_vertex_shader_batched(
    device_context_t* context,
    size_t shader_id,
    size_t vertex_offset,         // first (absolute) vertex of this run
    size_t vertex_count,          // vertices in this run
    size_t first_draw,            // first draw slot of this run
    size_t num_draws,             // one-past-last draw slot of this run
    size_t max_draw_vertices,
    int block_mode,              // 0: direct kernel (1-draw run); 1: block kernel (multi-draw run)
    int blocking_upload,
    cl_event* out_upload_event,   // async only: receives the upload-completion event (else NULL)
    const uint8_t*  arena,        // uniform base; uploads arena[first_draw .. first_draw+uniform_count)
    size_t uniform_count,         // unique uniform blobs in this run (run_draws for direct/identity)
    const vertex_attribute_data_t* attr_data_arena, // base; uploads [first_draw .. first_draw+vdata_count)
    size_t vdata_count,           // unique attribute-data blobs in this run (run_draws for direct/identity)
    const float (*vattrib_arena)[DEVICE_VERTEX_ATTRIBUTE_SIZE][4], // base; uploads [first_draw .. first_draw+vattrib_count)
    size_t vattrib_count,         // unique default-attribute-value blobs in this run (run_draws for direct/identity)
    const vertex_config_t* config,// base; uploads config[first_draw..num_draws) — per-draw arena ids
    const uint32_t* draw_start    // base, absolute; uploads draw_start[first_draw..num_draws]
) {
    if (vertex_count == 0 || num_draws <= first_draw) {
        if (out_upload_event) *out_upload_event = NULL;
        return;
    }

    cl_bool blk = blocking_upload ? CL_TRUE : CL_FALSE;

    cl_command_queue queue = __device_get_vertex_command_queue(context);

    __device_vertex_stage_wait(context, queue);

    __device_vertex_shader_data_t* vs = &context->device->shaders[shader_id].vertex;
    cl_kernel kernel = block_mode ? vs->kernel_block : vs->kernel_direct;

    size_t vertex_attribute_size = vs->number_attributes;
    cl_mem  attribute_pointer_mems [DEVICE_VERTEX_ATTRIBUTE_SIZE];

    size_t run_draws    = num_draws - first_draw;
    size_t attr_stride  = sizeof(vertex_attribute_data_t[DEVICE_VERTEX_ATTRIBUTE_SIZE]);
    size_t vattrib_stride = sizeof(cl_float4[DEVICE_VERTEX_ATTRIBUTE_SIZE]);
    cl_event* up_ev = (!blocking_upload && out_upload_event) ? out_upload_event : NULL;

    CL_CHECK(clEnqueueWriteBuffer(queue, context->g_uniform_arena,          blk, first_draw * DEVICE_UNIFORM_CAPACITY, uniform_count * DEVICE_UNIFORM_CAPACITY, arena + first_draw * DEVICE_UNIFORM_CAPACITY, 0, NULL, NULL));
    CL_CHECK(clEnqueueWriteBuffer(queue, context->g_attribute_data_arena,   blk, first_draw * attr_stride, vdata_count * attr_stride, attr_data_arena + first_draw * DEVICE_VERTEX_ATTRIBUTE_SIZE, 0, NULL, NULL));
    CL_CHECK(clEnqueueWriteBuffer(queue, context->g_vertex_attribute_arena, blk, first_draw * vattrib_stride, vattrib_count * vattrib_stride, vattrib_arena + first_draw, 0, NULL, block_mode ? NULL : up_ev));
    if (block_mode)
    {
        CL_CHECK(clEnqueueWriteBuffer(queue, context->g_config,           blk, first_draw * sizeof(vertex_config_t), run_draws * sizeof(vertex_config_t), config + first_draw, 0, NULL, NULL));
        CL_CHECK(clEnqueueWriteBuffer(queue, context->g_draw_start,       blk, first_draw * sizeof(cl_uint), (run_draws + 1) * sizeof(cl_uint), draw_start + first_draw, 0, NULL, up_ev));
    }
    if (out_upload_event && !up_ev) *out_upload_event = NULL;

    (void) max_draw_vertices;
    for (size_t attribute = 0; attribute < vertex_attribute_size; ++attribute)
    {
        __device_vertex_attribute_pointer_t *attribute_pointer = &context->vertex_attribute_pointers[attribute];
        cl_mem *pointer = &attribute_pointer_mems[attribute];

        if (attribute_pointer->is_host) {
            *pointer = __device_acquire_mem(&context->host_attr_snapshot[attribute], queue);
        }
        else
        {
            __device_mem_t* mem = &context->device->buffers[attribute_pointer->mem.device_id];
            *pointer = __device_acquire_mem(mem, queue);
        }
    }

    #ifndef NDEBUG
    {
        for (size_t attribute = 0; attribute < vertex_attribute_size; ++attribute)
        {
            vertex_attribute_data_t vad = attr_data_arena[first_draw * DEVICE_VERTEX_ATTRIBUTE_SIZE + attribute];
            if ((vad.misc & VERTEX_ATTRIBUTE_ACTIVE_POINTER) == 0) continue; // default-value path: no buffer read

            size_t comps    = gl_get_vertex_attribute_size(vad);
            size_t draw_extent = block_mode ? max_draw_vertices : vertex_count;
            if (draw_extent == 0) continue;
            size_t max_read = (size_t) vad.offset + (draw_extent - 1) * (size_t) vad.stride + comps * 4u; // 4 = max bytes/component
            size_t buf_size = 0;
            CL_CHECK(clGetMemObjectInfo(attribute_pointer_mems[attribute], CL_MEM_SIZE, sizeof(buf_size), &buf_size, NULL));

            if (max_read > buf_size)
            {
                printf("ATTR-OOB: inst=%zu attr=%zu active_ptr off=%u stride=%u comps=%zu draw_extent=%zu -> max_read=%zu > buf_size=%zu (is_host=%zu device_id=%zu)\n",
                    first_draw, attribute, vad.offset, vad.stride, comps, draw_extent, max_read, buf_size,
                    context->vertex_attribute_pointers[attribute].is_host,
                    context->vertex_attribute_pointers[attribute].is_host ? (size_t) 0 : context->vertex_attribute_pointers[attribute].mem.device_id);
            }
        }
    }
    #endif

    if (block_mode)
    {
        __device_set_vertex_shader_block_args(
            kernel, context, vertex_attribute_size,
            context->g_vertex_attribute_arena, // per-draw default attribute values, uploaded above
            context->g_attribute_data_arena,   // per-draw attribute_data (offsets), uploaded above
            context->g_uniform_arena,
            context->g_draw_start,
            context->g_config,
            attribute_pointer_mems,
            vertex_offset + vertex_count,  // c_num_vertices: vid guard (run-end vertex)
            first_draw,                    // c_first_draw: run's first draw slot
            num_draws,                     // c_num_draws: instance guard + sentinel index
            vertex_offset                  // c_vertex_offset: draw-local base
        );
    }
    else
    {
        __device_set_vertex_shader_direct_args(
            kernel, context, vertex_attribute_size,
            context->g_vertex_attribute_arena,
            context->g_attribute_data_arena,
            context->g_uniform_arena,
            attribute_pointer_mems,
            vertex_offset + vertex_count,  // c_num_vertices: vid guard
            vertex_offset,                 // c_vertex_offset: draw-local base
            config[first_draw]             // c_config: this draw's {vattrib,vdata,uniform} ids
        );
    }

    size_t lws[] = {DEVICE_VERTEX_THREADS};
    size_t gwo[] = {0};
    size_t gws[] = {lws[0] * ((vertex_count - 1)/lws[0] + 1)};

    cl_event wait_event;
    #ifndef NDEBUG
    {
        printf("%s[%s]: gwo={%zu}, gws={%zu}, lws={%zu}, draws=[%zu,%zu), voff=%zu, vcnt=%zu\n",
            __func__, block_mode ? "block" : "direct",
            gwo[0], gws[0], lws[0], first_draw, num_draws, vertex_offset, vertex_count);
    }
    #endif
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 1, gwo, gws, lws, 0, NULL, &wait_event));
    #ifndef NDEBUG
    {
        CL_CHECK(clFinish(queue));
    }
    #endif

    __device_vertex_stage_publish(context, wait_event);

    // Release host pointers and synchronize device pointers

    for (size_t attribute = 0; attribute < vertex_attribute_size; ++attribute)
    {
        __device_vertex_attribute_pointer_t *attribute_pointer = &context->vertex_attribute_pointers[attribute];

        if (attribute_pointer->is_host)
        {
            __device_barrier_mem(&context->host_attr_snapshot[attribute], wait_event);
        }
        else
        {
            __device_mem_t* mem = &context->device->buffers[attribute_pointer->mem.device_id];
            __device_barrier_mem(mem, wait_event);
        }
    }

    CL_CHECK(clReleaseEvent(wait_event));

    // __device_print_vertex_shader_output(context, queue, total_vertices, /*num_varying=*/1);
}

void device_launch_range_triangle_assembly(
    device_context_t* context, 
    size_t shader_id,
    render_mode_t mode, 
    size_t frag_config_id, 
    size_t vertex_offset, size_t triangle_offset, size_t num_triangles,
    size_t width, size_t height, size_t size, const uint16_t* ptr
) {
    cl_kernel kernel = context->device->triangle_setup_range_kernel;

    if (num_triangles == 0) return;

    size_t qidx = __device_get_vertex_command_index(context);
    cl_command_queue queue = __device_get_vertex_command_queue(context);

    __device_vertex_stage_wait(context, queue);

    cl_mem g_index_buffer;
    size_t index_bytes = sizeof(cl_ushort[size]);
    int    index_buffer_to_device = index_bytes <= DEVICE_INDEX_BUFFER_CACHE_BYTES;

    __device_mem_t* index_mem;
    if (index_buffer_to_device)
    {
        index_mem = &context->index_buffer_cache[qidx];
        __device_write_mem(index_mem, CL_TRUE, 0, index_bytes, ptr);
        g_index_buffer = __device_acquire_mem(index_mem, queue);
    }
    else
    {
        CL_ASSIGN_CHECK(g_index_buffer, clCreateBuffer(
            context->device->context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, index_bytes, (void*) ptr, error
        ));
    }

    __device_acquire_mem(&context->a_num_subtris,  queue);

    __device_set_triangle_setup_range_kernel_args(
        kernel,
        context,
        g_index_buffer,
        num_triangles,
        vertex_offset,
        mode,
        context->device->shaders[shader_id].vertex.vertex_size,
        frag_config_id,
        height, 
        width
    );

    // size_t gwo = triangle_offset;
    // size_t gws = num_triangles;
    size_t lws[] = {DEVICE_SETUP_THREADS};
    size_t gwo[] = {triangle_offset};
    size_t gws[] = {lws[0] * ((num_triangles-1)/lws[0] + 1)};

    cl_event wait_event;

    #ifndef NDEBUG
    {
        printf("%s: gwo={%zu}, gws={%zu}, lws={%zu}\n", __func__, gwo[0], gws[0], lws[0]);
    }
    #endif
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 1, gwo, gws, lws, 0, NULL, &wait_event));

    __device_vertex_stage_publish(context, wait_event);

    if (index_buffer_to_device) __device_barrier_mem(index_mem, wait_event);
    else                        CL_CHECK(clReleaseMemObject(g_index_buffer));

    CL_CHECK(clEnqueueBarrierWithWaitList(context->raster_command_queue, 1, &wait_event, NULL));
    CL_CHECK(clReleaseEvent(wait_event));

    __device_advance_vertex_command_index(context);

    // __device_print_triangle_assembly_output(context, queue, num_triangles);
}

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
) {
    cl_kernel kernel = context->device->triangle_setup_arrays_kernel;

    if (num_triangles == 0) return;

    cl_command_queue queue = __device_get_vertex_command_queue(context);

    __device_vertex_stage_wait(context, queue);

    __device_acquire_mem(&context->a_num_subtris,   queue);

    __device_set_triangle_setup_arrays_kernel_args(
        kernel,
        context,
        num_triangles,
        vertex_offset,
        mode,
        context->device->shaders[shader_id].vertex.vertex_size,
        frag_config_id,
        height,
        width
    );

    size_t lws[] = {DEVICE_SETUP_THREADS};
    size_t gwo[] = {triangle_offset};
    size_t gws[] = {lws[0] * ((num_triangles-1)/lws[0] + 1)};

    cl_event wait_event;
    
    #ifndef NDEBUG
    {
        printf("%s: gwo={%zu}, gws={%zu}, lws={%zu}\n", __func__, gwo[0], gws[0], lws[0]);
    }
    #endif
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 1, gwo, gws, lws, 0, NULL, &wait_event));

    __device_vertex_stage_publish(context, wait_event);

    CL_CHECK(clEnqueueBarrierWithWaitList(context->raster_command_queue, 1, &wait_event, NULL));
    CL_CHECK(clReleaseEvent(wait_event));

    __device_advance_vertex_command_index(context);

    // __device_print_triangle_assembly_output(context, queue, num_triangles);
}

static void __device_set_triangle_setup_arrays_block_kernel_args(
    cl_kernel kernel,
    device_context_t* context,
    cl_uint c_first_draw,
    cl_uint c_num_draws,
    cl_uint c_vertex_size,
    cl_uint c_viewport_height,
    cl_uint c_viewport_width
) {
    cl_uint c_max_subtris = __device_get_max_number_subtriangles();
    cl_uint c_samples_log2 = 0; // TODO: not impl
    cl_mem  t_vertex_buffer = __device_get_texture_vertex_buffer(context);

    cl_uint arg_idx = 0;

    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem),            &context->a_num_subtris));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem),            &context->g_tri_header));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem),            &context->g_tri_data));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem),            &context->g_tri_subtris));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem),            &t_vertex_buffer));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(cl_mem),            &context->g_setup_arena));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_first_draw),      &c_first_draw));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_num_draws),       &c_num_draws));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_max_subtris),     &c_max_subtris));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_samples_log2),    &c_samples_log2));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_vertex_size),     &c_vertex_size));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_viewport_height), &c_viewport_height));
    CL_CHECK(clSetKernelArg(kernel, arg_idx++, sizeof(c_viewport_width),  &c_viewport_width));}

void device_launch_arrays_triangle_assembly_block(
    device_context_t* context,
    size_t shader_id,
    size_t first_draw,
    size_t num_draws,
    size_t width,
    size_t height,
    const setup_draw_config_t* setup_arena
) {
    if (num_draws <= first_draw) return;

    size_t num_tris = setup_arena[num_draws].tri_start - setup_arena[first_draw].tri_start;
    if (num_tris == 0) return;

    cl_kernel kernel = context->device->triangle_setup_arrays_block_kernel;
    cl_command_queue queue = __device_get_vertex_command_queue(context);

    __device_vertex_stage_wait(context, queue);

    __device_acquire_mem(&context->a_num_subtris, queue);

    size_t run_draws = num_draws - first_draw;

    CL_CHECK(clEnqueueWriteBuffer(
        queue, context->g_setup_arena, CL_TRUE,
        first_draw * sizeof(setup_draw_config_t),
        (run_draws + 1) * sizeof(setup_draw_config_t),
        setup_arena + first_draw, 0, NULL, NULL));

    __device_set_triangle_setup_arrays_block_kernel_args(
        kernel,
        context,
        first_draw,
        num_draws,
        context->device->shaders[shader_id].vertex.vertex_size,
        height,
        width
    );

    // Dense over the run's triangles; each lane binary-searches g_setup[].tri_start
    // for its draw (gwo=0 — the kernel folds the run's tri_start offset itself).
    size_t lws[] = {DEVICE_SETUP_THREADS};
    size_t gws[] = {lws[0] * ((num_tris - 1)/lws[0] + 1)};

    cl_event wait_event;

    #ifndef NDEBUG
    {
        printf("%s: gws={%zu}, lws={%zu}, draws=[%zu,%zu), tris=%zu\n", __func__, gws[0], lws[0], first_draw, num_draws, num_tris);
    }
    #endif
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 1, NULL, gws, lws, 0, NULL, &wait_event));

    // Reads g_setup_arena + g_vertex_buffer: gate the next vertex-stage launch.
    __device_vertex_stage_publish(context, wait_event);

    CL_CHECK(clEnqueueBarrierWithWaitList(context->raster_command_queue, 1, &wait_event, NULL));
    CL_CHECK(clReleaseEvent(wait_event));

    __device_advance_vertex_command_index(context);
}

void device_launch_bin_dispatch(
    device_context_t* context, 
    size_t num_triangles, 
    size_t width, 
    size_t height
) {
    cl_kernel kernel = context->device->bin_raster_kernel;
    cl_command_queue queue = context->raster_command_queue;

    __device_acquire_mem(&context->a_bin_counter, queue),
    __device_acquire_mem(&context->a_num_bin_segs, queue),

    __device_set_bin_raster_kernel_args(
        kernel,
        context,
        num_triangles,
        height, 
        width
    );

    size_t lws[2] = {DEVICE_SUB_GROUP_THREADS, DEVICE_BIN_SUB_GROUPS};
    size_t gws[2] = {lws[0] * CR_BIN_STREAMS_SIZE, lws[1]};

    cl_event wait_event;

    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 2, NULL, gws, lws, 0, NULL, &wait_event));
    #ifndef NDEBUG
    {
        cl_int bin_segs = 0;
        cl_int bin_total[CR_MAXBINS_SQR][CR_BIN_STREAMS_SIZE];
        CL_CHECK(clEnqueueReadBuffer(queue, context->a_num_bin_segs.mem, CL_TRUE, 0, sizeof(bin_segs), &bin_segs, 0, NULL, NULL));
        CL_CHECK(clEnqueueReadBuffer(queue, context->g_bin_total, CL_TRUE, 0, sizeof(bin_total), &bin_total, 0, NULL, NULL));
        printf("bin_segs=%d, max_bin_segs=%ld\n", bin_segs, __device_get_max_number_bin_segments());

        int sum_bin_total = 0;
        for(int bin_idx = 0; bin_idx < CR_MAXBINS_SQR; ++bin_idx) {
            int total = 0;
            for(int stream_idx = 1; stream_idx < CR_BIN_STREAMS_SIZE; ++stream_idx) {
                total += bin_total[bin_idx][stream_idx];
            }
            if (total) printf("bin_total[%d]=%d\n", bin_idx, total);
            sum_bin_total += total;
        }
        printf("sum(bin_total)=%d\n", sum_bin_total);
        if (bin_segs > __device_get_max_number_bin_segments())
        {
            printf("ERROR: bin segs > max_bin_segs\n");
            exit(1);
        }
    }
    #endif

    __device_barrier_mem(&context->a_bin_counter,   wait_event);

    CL_CHECK(clReleaseEvent(wait_event));

    __device_reset_vertex_command_index(context);

    __device_reset_mem(&context->a_bin_counter,    sizeof(cl_int), &ZERO);
}

void device_launch_tile_dispatch(
    device_context_t* context,
    uint32_t deferred_clear,
    size_t width, 
    size_t height
) {
    cl_kernel kernel = context->device->coarse_raster_kernel;
    cl_command_queue queue = context->raster_command_queue;

    __device_acquire_mem(&context->a_coarse_counter,    queue),
    __device_acquire_mem(&context->a_num_active_tiles,  queue),
    __device_acquire_mem(&context->a_num_tile_segs,     queue),

    __device_set_coarse_raster_kernel_args(
        kernel,
        context,
        deferred_clear,
        height,
        width
    );

    /*
    cl_int a_bin_counter, a_num_bin_segs;
    __device_read_mem(&context->a_bin_counter, CL_TRUE, 0, sizeof(a_bin_counter), &a_bin_counter);
    __device_read_mem(&context->a_num_bin_segs, CL_TRUE, 0, sizeof(a_num_bin_segs), &a_num_bin_segs);
    printf("a_bin_counter=%d, a_num_bin_segs=%d\n", a_bin_counter, a_num_bin_segs);
    */

    size_t lws[2] = {DEVICE_SUB_GROUP_THREADS, DEVICE_COARSE_SUB_GROUPS};
    size_t gws[2] = {lws[0] * DEVICE_NUM_CORES, lws[1]};

    #if DEVICE_BIN_QUEUE_SIZE > 1
    #error "DEVICE_BIN_QUEUE_SIZE must be 1 for now, because of the way we handle events"
    #endif

    for (size_t w=0; w<DEVICE_BIN_QUEUE_SIZE; ++w) 
    {
        for (size_t h=0; h<DEVICE_BIN_QUEUE_SIZE; ++h) 
        {
            cl_event wait_event;

            CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 2, NULL, gws, lws, 0, NULL, &wait_event));
            #ifndef NDEBUG
            {
                cl_int tile_segs = 0, active_tiles = 0;
                CL_CHECK(clEnqueueReadBuffer(queue, context->a_num_tile_segs.mem, CL_TRUE, 0, sizeof(tile_segs), &tile_segs, 0, NULL, NULL));
                CL_CHECK(clEnqueueReadBuffer(queue, context->a_num_active_tiles.mem, CL_TRUE, 0, sizeof(active_tiles), &active_tiles, 0, NULL, NULL));
                printf("tile_segs=%d, max_tile_segs=%ld\n", tile_segs, __device_get_max_number_tile_segments());
                printf("active_tiles=%d\n", active_tiles);
            }
            #endif

            __device_barrier_mem(&context->a_coarse_counter, wait_event);
            context->bin_wait_event[w][h] = wait_event;
        }
    }

    __device_reset_mem(&context->a_coarse_counter,    sizeof(cl_int), &ZERO);
}

void device_launch_fragment_shader(
    device_context_t* context,
    size_t shader_id,
    clear_data_t c_data,
    enabled_data_t c_enabled,
    size_t colorbuffer_id, size_t depthbuffer_id, size_t stencilbuffer_id,
    uint32_t colorbuffer_mode,
    size_t width, size_t height, 
    size_t bin_queue_id
) {
    cl_kernel kernel = context->device->shaders[shader_id].fragment;

    gl_framebuffer_data_t framebuffer_data = {0};

    __device_set_framebuffer_data(
        &framebuffer_data,
        colorbuffer_id,
        depthbuffer_id,
        stencilbuffer_id
    );

    __device_set_fragment_shader_kernel_args(
        kernel,
        context,
        __device_get_texture_rt_mem(context->device, colorbuffer_id),
        __device_get_texture_rt_mem(context->device, depthbuffer_id),
        __device_get_texture_rt_mem(context->device, stencilbuffer_id),
        c_data,
        c_enabled,
        colorbuffer_mode,
        height,
        width,
        framebuffer_data
    );

    size_t lws[2] = {DEVICE_SUB_GROUP_THREADS, DEVICE_FINE_SUB_GROUPS};

    size_t global_instances;
    #if (DEVICE_SUB_GROUP_INTRINSICTS_SUPPORT == 1)
        global_instances = DEVICE_NUM_CORES;
    #else
        global_instances = DEVICE_NUM_CORES * 20; // TODO: this could be improve by some heuristic
    #endif

    size_t gws[2] = {lws[0] * global_instances, lws[1]};

    __device_bin_queue_t *bin_queue = __device_get_bin_queue(context->device, bin_queue_id);

    for (size_t w=0; w<DEVICE_BIN_QUEUE_SIZE; ++w) 
    {
        for (size_t h=0; h<DEVICE_BIN_QUEUE_SIZE; ++h)
        {
            cl_command_queue queue = bin_queue->queues[w][h];

            __device_acquire_mem(&context->a_fine_counter, queue);
            __device_acquire_mem(&context->fragment_texture_datas_mem, queue);
            __device_acquire_mem(&context->fragment_uniform_mem, queue);
            __device_acquire_mem(&context->rop_configs_mem, queue);

            cl_event bin_wait_event = context->bin_wait_event[w][h];
            cl_event wait_event;

            CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 2, NULL, gws, lws, 1, &bin_wait_event, &wait_event));
            #ifndef NDEBUG
            {
                CL_CHECK(clFinish(queue));
            }
            #endif

            CL_CHECK(clReleaseEvent(bin_wait_event));
        
            __device_barrier_mem(&context->a_fine_counter,              wait_event);
            __device_barrier_mem(&context->fragment_texture_datas_mem,  wait_event);
            __device_barrier_mem(&context->fragment_uniform_mem,        wait_event);
            __device_barrier_mem(&context->rop_configs_mem,             wait_event);

            __device_barrier_mem(&context->a_num_active_tiles,  wait_event);
            __device_barrier_mem(&context->a_num_bin_segs,      wait_event);
            __device_barrier_mem(&context->a_num_subtris,       wait_event);
            __device_barrier_mem(&context->a_num_tile_segs,     wait_event);

            for (size_t vertex_queue = 0; vertex_queue < DEVICE_VERTEX_COMMAND_QUEUE_SIZE; ++vertex_queue) 
            {
                cl_command_queue vertex_command_queue = context->vertex_command_queues[vertex_queue];
                CL_CHECK(clEnqueueBarrierWithWaitList(vertex_command_queue, 1, &wait_event, NULL));
            }
            
            CL_CHECK(clReleaseEvent(wait_event));
        }
    }

    // reset state
    __device_reset_mem(&context->a_fine_counter,        sizeof(cl_int),  &ZERO);
    __device_reset_mem(&context->a_num_active_tiles,    sizeof(cl_int),  &ZERO);
    __device_reset_mem(&context->a_num_bin_segs,        sizeof(cl_int),  &ZERO);
    __device_reset_mem(&context->a_num_subtris,         sizeof(cl_uint), &MAX_NUMBER_TRIANGLES);
    __device_reset_mem(&context->a_num_tile_segs,       sizeof(cl_int),  &ZERO);
}

void device_launch_clear_framebuffer(
    device_t* device,
    clear_data_t c_data,
    enabled_data_t c_enabled,
    size_t colorbuffer_id, size_t depthbuffer_id, size_t stencilbuffer_id,
    uint32_t colorbuffer_mode,
    size_t width, size_t height, 
    size_t bin_queue_id
) {
    cl_kernel kernel = device->clear_kernel;
    __device_bin_queue_t* bin_queue = __device_get_bin_queue(device, bin_queue_id);

    cl_uint c_colorbuffer_mode = colorbuffer_mode;
    cl_uint c_viewport_width = width;

    cl_uint count = 0;

    cl_mem rt_color   = __device_get_texture_rt_mem(device, colorbuffer_id);
    cl_mem rt_depth   = __device_get_texture_rt_mem(device, depthbuffer_id);
    cl_mem rt_stencil = __device_get_texture_rt_mem(device, stencilbuffer_id);
    CL_CHECK(clSetKernelArg(kernel, count++, sizeof(cl_mem), &rt_color));
    CL_CHECK(clSetKernelArg(kernel, count++, sizeof(cl_mem), &rt_depth));
    CL_CHECK(clSetKernelArg(kernel, count++, sizeof(cl_mem), &rt_stencil));
    #ifndef DEVICE_RW_IMAGE_ENABLED
    {
        CL_CHECK(clSetKernelArg(kernel, count++, sizeof(c_colorbuffer_mode), &c_colorbuffer_mode));
        CL_CHECK(clSetKernelArg(kernel, count++, sizeof(c_viewport_width), &c_viewport_width));
    }
    #endif
    CL_CHECK(clSetKernelArg(kernel, count++, sizeof(c_data), &c_data));
    CL_CHECK(clSetKernelArg(kernel, count++, sizeof(c_enabled), &c_enabled));

    size_t global_work_offset[2] = {0, 0};
    size_t global_work_size[2] = {width, height};
    
    CL_CHECK(clEnqueueNDRangeKernel(bin_queue->queues[0][0], kernel, 2, global_work_offset, global_work_size, NULL, 0, NULL, NULL));
}

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
) {
    cl_kernel kernel = device->read_pixels_kernel;

    cl_mem colorbuffer = __device_get_texture_rt_mem(device, colorbuffer_id);
    
    size_t size = __device_get_bytes_from_texture_mode(ptr_mode) * (width - x) * (height - y);

    cl_mem buffer;
    CL_ASSIGN_CHECK(buffer, clCreateBuffer(
        device->context,
        CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR,
        size,
        ptr,
        error
    ));
    

    __device_set_read_pixels_kernel_args(
        kernel, 
        colorbuffer,
        buffer,
        colorbuffer_mode,
        ptr_mode,
        swap_rb,
        swap_y
    );

    __device_bin_queue_t* bin_queue = __device_get_bin_queue(device, bin_queue_id);
    
    cl_command_queue queue = bin_queue->queues[0][0];
    size_t global_work_offset[2] = {x, y};
    size_t global_work_size[2] = {width, height};
    
    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 2, global_work_offset, global_work_size, NULL, 0, NULL, NULL));
    void* map_ptr;
    CL_ASSIGN_CHECK(map_ptr, clEnqueueMapBuffer(queue, buffer, CL_TRUE, CL_MAP_READ, 0, size, 0, NULL, NULL, error));
    // CL_CHECK(clEnqueueReadBuffer(queue, buffer, CL_TRUE, 0, size, ptr, 0, NULL, NULL));
    CL_CHECK(clReleaseMemObject(buffer));
}

void device_wait_bin_queue(
    device_t* device,
    size_t bin_queue_id
) {
    __device_bin_queue_t* bin_queues = __device_get_bin_queue(device, bin_queue_id);

    for(int i=0; i<DEVICE_BIN_QUEUE_SIZE; ++i) 
    {
        for(int j=0; j<DEVICE_BIN_QUEUE_SIZE; ++j)
        {
            cl_command_queue queue = bin_queues->queues[i][j];

            CL_CHECK(clFinish(queue));
        }
    }
}

//-------------------------------------------------------------------------------------

void device_bind_buffer_to_vertex_attribute_pointer(
    device_context_t* context, 
    size_t attribute_id, 
    size_t buffer_id
) {
    context->vertex_attribute_pointers[attribute_id] = (__device_vertex_attribute_pointer_t) {
        .is_host = 0,
        .mem = {
            .device_id = buffer_id
        }
    };
}

void device_bind_host_pointer_to_vertex_attribute_pointer(
    device_context_t* context, 
    size_t attribute_id,
    size_t stride,
    void* ptr
) {
    context->vertex_attribute_pointers[attribute_id] = (__device_vertex_attribute_pointer_t) {
        .stride = stride,
        .mem = {
            .host = ptr
        }
    };
}

void device_bind_texture_unit(device_context_t* context, size_t unit_index, size_t texture_id) 
{
    context->texture_units_ids[unit_index] = texture_id;
}

//-------------------------------------------------------------------------------------

void device_copy_context_last_state(device_context_t* dst, device_context_t* src)
{
    memcpy(dst->texture_units_ids, src->texture_units_ids, sizeof(src->texture_units_ids));
    memcpy(dst->vertex_attribute_pointers, src->vertex_attribute_pointers, sizeof(src->vertex_attribute_pointers));
    #ifdef DEVICE_IMAGE_ENABLED
    memcpy(dst->fragment_texture_samplers, src->fragment_texture_samplers, sizeof(src->fragment_texture_samplers));
    #endif

    __device_reset_mem(&dst->a_bin_counter,         sizeof(ZERO), &ZERO);
    __device_reset_mem(&dst->a_coarse_counter,      sizeof(ZERO), &ZERO);
    __device_reset_mem(&dst->a_fine_counter,        sizeof(ZERO), &ZERO);
    __device_reset_mem(&dst->a_num_active_tiles,    sizeof(ZERO), &ZERO);
    __device_reset_mem(&dst->a_num_bin_segs,        sizeof(ZERO), &ZERO);
    __device_reset_mem(&dst->a_num_subtris,         sizeof(MAX_NUMBER_TRIANGLES), &MAX_NUMBER_TRIANGLES);
    __device_reset_mem(&dst->a_num_tile_segs,       sizeof(ZERO), &ZERO);

    dst->vertex_attribute_data_index = 0;
    dst->vertex_uniform_index = 0;

}

//-------------------------------------------------------------------------------------

void device_wait_event(device_event_t event)
{
    if (!event) return;
    CL_CHECK(clWaitForEvents(1, &event));
    CL_CHECK(clReleaseEvent(event));
}

void device_release_event(device_event_t event)
{
    if (!event) return;
    CL_CHECK(clReleaseEvent(event));
}

device_event_t device_write_vertex_attribute_data(
    device_context_t* context,
    vertex_attribute_data_t vertex_attribute_data[DEVICE_VERTEX_ATTRIBUTE_SIZE]
) {
    context->vertex_attribute_data_index += 1;
    context->vertex_attribute_data_index %= DEVICE_VERTEX_COMMAND_QUEUE_SIZE;

    size_t idx = context->vertex_attribute_data_index;

    return __device_write_mem(
        &context->vertex_attribute_data_mem[idx],
        CL_FALSE,
        0,
        sizeof(vertex_attribute_data_t[DEVICE_VERTEX_ATTRIBUTE_SIZE]),
        vertex_attribute_data
    );
}


device_event_t device_write_vertex_uniform(
    device_context_t* context,
    uint8_t uniform_data[DEVICE_UNIFORM_CAPACITY],
    int blocking_write
) {
    context->vertex_uniform_index += 1;
    context->vertex_uniform_index %= DEVICE_VERTEX_COMMAND_QUEUE_SIZE;

    size_t idx = context->vertex_uniform_index;

    return __device_write_mem(
        &context->vertex_uniform_mem[idx],
        blocking_write,
        0,
        sizeof(cl_char[DEVICE_UNIFORM_CAPACITY]),
        uniform_data
    );
}

device_event_t device_write_rop_config(
    device_context_t* context,
    size_t primitive_id,
    rop_config_t* rop_config
) {
    size_t size = sizeof(rop_config_t);
    size_t offset = primitive_id * size;

    return __device_write_mem(
        &context->rop_configs_mem,
        CL_FALSE,
        offset,
        size,
        rop_config
    );
}

// TODO: textures are not implemented for vertex
device_event_t device_write_fragment_texture_datas(
    device_context_t* context,
    const texture_data_t texture_datas[DEVICE_TEXTURE_UNITS]
) {
    size_t size = sizeof(texture_data_t[DEVICE_TEXTURE_UNITS]);

    device_event_t write_event = __device_write_mem(
        &context->fragment_texture_datas_mem,
        CL_FALSE,
        0,
        size,
        texture_datas
    );

    #ifdef DEVICE_IMAGE_ENABLED
    for (size_t unit = 0; unit < DEVICE_TEXTURE_UNITS; ++unit)
    {
        context->fragment_texture_samplers[unit] =
            __device_select_sampler(context->device, texture_datas[unit]);
    }
    #endif

    return write_event;
}

device_event_t device_write_fragment_uniform(
    device_context_t* context,
    size_t primitive_id,
    const uint8_t uniform_data[DEVICE_UNIFORM_CAPACITY]
) {
    size_t size = sizeof(uint8_t[DEVICE_UNIFORM_CAPACITY]);
    size_t offset = primitive_id * size;

    return __device_write_mem(
        &context->fragment_uniform_mem,
        CL_FALSE,
        offset,
        size,
        uniform_data
    );
}

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
) 
{
    __device_mem_t* texture_mem = __device_get_texture_sample_obj(device, texture_id);

    size_t origin[3] = {x, y, 0};
    size_t region[3] = {width, height, 1};

    __device_write_2d_texture(texture_mem, CL_TRUE, origin, region, 0, 0, data);
}

void device_write_buffer(device_t* device, size_t buffer_id, size_t offset, size_t size, const void* data) 
{
    __device_write_mem(&device->buffers[buffer_id], CL_TRUE, offset, size, data);
}

void device_read_buffer(device_t* device, size_t buffer_id, size_t offset, size_t size, void* data) 
{
    __device_read_mem(&device->buffers[buffer_id], CL_TRUE, offset, size, data);
}
