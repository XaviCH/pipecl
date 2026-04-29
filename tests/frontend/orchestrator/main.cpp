#include <frontend/orchestrator.h>
#include <tests/glsc2/common.h>
#include <chrono>

#define SHADER_PATH "kernel.cl.o"
#define SHADER2_PATH "kernel2.cl.o"
#define WIDTH 1920
#define HEIGHT 1080

float triangle_positions[] = {
    0.0f,  1.0f, 0.0f,
   -1.0f, -1.0f, 0.0f,
    1.0f, -1.0f, 0.0f,
};

size_t triangle_positions_stride = sizeof(triangle_positions[0]) * 3;

float triangle_colors[] = {
    1.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 1.0f,
};

size_t triangle_colors_stride = sizeof(triangle_colors[0]) * 3;

float triangle_coords[] = {
    0.0f,   1.0f,
    0.0f,   0.0f,
    1.0f,   0.0f,
};

size_t triangle_coords_stride = sizeof(triangle_coords[0]) * 2;

int main() {    
    orch_handler_t orch;
    orch_init(&orch);

    size_t framebuffer_id_0 = orch_create_framebuffer(&orch);
    size_t colorbuffet_id_0 = orch_create_renderbuffer(&orch, WIDTH, HEIGHT, TEX_RGBA8);
    size_t depthbuffer_id_0 = orch_create_renderbuffer(&orch, WIDTH, HEIGHT, TEX_DEPTH_COMPONENT16);
    size_t stencilbuffer_id_0 = orch_create_renderbuffer(&orch, WIDTH, HEIGHT, TEX_STENCIL_INDEX8);

    size_t framebuffer_id_1 = orch_create_framebuffer(&orch);
    size_t colorbuffet_id_1 = orch_create_renderbuffer(&orch, WIDTH, HEIGHT, TEX_RGBA8);
    size_t depthbuffer_id_1 = orch_create_renderbuffer(&orch, WIDTH, HEIGHT, TEX_DEPTH_COMPONENT16);
    size_t stencilbuffer_id_1 = orch_create_renderbuffer(&orch, WIDTH, HEIGHT, TEX_STENCIL_INDEX8);

    orch_attach_render_colorbuffer(&orch, framebuffer_id_0, colorbuffet_id_0);
    orch_attach_render_depthbuffer(&orch, framebuffer_id_0, depthbuffer_id_0);
    orch_attach_render_stencilbuffer(&orch, framebuffer_id_0, stencilbuffer_id_0);

    orch_attach_render_colorbuffer(&orch, framebuffer_id_1, colorbuffet_id_1);
    orch_attach_render_depthbuffer(&orch, framebuffer_id_1, depthbuffer_id_1);
    orch_attach_render_stencilbuffer(&orch, framebuffer_id_1, stencilbuffer_id_1);

    file_t file;
    read_file(SHADER_PATH, &file);
    size_t shader_id = orch_create_shader_from_binary(&orch, file.size, file.data);

    file_t file2;
    read_file(SHADER2_PATH, &file2); 
    size_t shader_id2 = orch_create_shader_from_binary(&orch, file2.size, file2.data);

    // vertex shader data
    size_t position_buffer_id = orch_create_buffer(&orch, sizeof(triangle_positions));
    orch_write_buffer(&orch, position_buffer_id, 0, sizeof(triangle_positions), triangle_positions);

    vertex_attribute_data_t vertex_attribute_data[DEVICE_VERTEX_ATTRIBUTE_SIZE];

    ppm_image_t* image = read_ppm("../../assets/dog.ppm");

    uint8_t* data = (uint8_t*) malloc(image->x*image->y*sizeof(uint8_t[4]));
    for(uint32_t i=0; i<image->x*image->y; ++i) {
        data[i*4+0] = image->data[i].red;
        data[i*4+1] = image->data[i].green;
        data[i*4+2] = image->data[i].blue;
        data[i*4+3] = 0x0Fu;
    }

    size_t texture_id = orch_create_2d_texture(&orch, image->x, image->y, TEX_RGBA8);
    orch_write_2d_texture(&orch, texture_id, 0, 0, image->x, image->y, TEX_RGBA8, data);
    texture_data_t texture_data[DEVICE_TEXTURE_UNITS];
    texture_data[0] = {
        .size = {(unsigned short)image->x, (unsigned short)image->y}
    };
    set_texture_data_mode(&texture_data[0], TEX_RGBA8);
    set_texture_data_wrap_s(&texture_data[0], TEXTURE_WRAP_REPEAT);
    set_texture_data_wrap_t(&texture_data[0], TEXTURE_WRAP_REPEAT);
    set_texture_data_min_filter(&texture_data[0], TEXTURE_FILTER_NEAREST);
    set_texture_data_mag_filter(&texture_data[0], TEXTURE_FILTER_NEAREST);
    // fragment shader data

    uint8_t uniform[DEVICE_UNIFORM_CAPACITY];

    render_mode_t mode = {
        .flags =  RENDER_MODE_FLAG_ENABLE_LERP
    };
    blending_data_t blending_data = {0};
    rgba8_t blending_color = get_rgba8(0, 0, 0, 255);
    depth_data_t depth_data = get_depth_data(DEPTH_FUNC_LESS, 0, -1);
    gl_stencil_data_t stencil_data = {0};
    enabled_data_t enabled_data = get_enabled_data(1,1,1,1,1,0xFF);

    rop_config_t rop_config = {
        .render_mode = mode,
        .blending_data = blending_data,
        .blending_color = blending_color,
        .stencil_data = stencil_data.front_misc,
        .depth_data = depth_data.misc,
        .enabled_data = enabled_data,
    };

    clear_data_t clear_data = {
        .color = get_rgba8(255, 0, 0, 255),
        .depth = {0xFFFFu},
        .stencil = {0},
    };

    // Queries
    void* host_colorbuffers = malloc(sizeof(rgba8_t[2][WIDTH][HEIGHT]));

    auto begin = std::chrono::high_resolution_clock::now();

    int samples = 1;
    for(int i=0; i<samples; ++i)
    {
        size_t framebuffer_id = i % 2 ? framebuffer_id_0 : framebuffer_id_1;

        orch_clear(&orch, framebuffer_id, clear_data, enabled_data);

        orch_write_fragment_data(&orch, framebuffer_id, uniform, rop_config);
        orch_attach_texture_unit(&orch, framebuffer_id, 0, texture_id);
        orch_write_fragment_texture_data(&orch, framebuffer_id, texture_data);

        orch_attach_vertex_attribute_ptr(&orch, framebuffer_id, 0, position_buffer_id);
        // orch_attach_vertex_attribute_ptr(&orch, framebuffer_id, 1, position_buffer_id);
        orch_attach_vertex_attribute_host_ptr(&orch, framebuffer_id, 1, triangle_colors_stride, triangle_colors);

        set_vertex_attribute(&vertex_attribute_data[0], 0, triangle_positions_stride, VERTEX_ATTRIBUTE_TYPE_FLOAT, VERTEX_ATTRIBUTE_SIZE_3, 0, 1);
        set_vertex_attribute(&vertex_attribute_data[1], 0, triangle_colors_stride, VERTEX_ATTRIBUTE_TYPE_FLOAT, VERTEX_ATTRIBUTE_SIZE_3, 0, 1);
        
        orch_write_vertex_attribute_data(&orch, framebuffer_id, vertex_attribute_data);
        
        for(int j=0; j<2000; ++j) {
            orch_draw_arrays(&orch, framebuffer_id, shader_id, mode, 0, 3);
        }
        //-------------------------------
        
        /*
        uint16_t index[3] = {0, 1, 2};
        *(cl_uint*)uniform = 0;

        orch_write_fragment_data(&orch, framebuffer_id, uniform, rop_config);

        orch_attach_vertex_attribute_host_ptr(&orch, framebuffer_id, 0, triangle_coords_stride, triangle_coords);
        orch_attach_vertex_attribute_ptr(&orch, framebuffer_id, 1, position_buffer_id);
        set_vertex_attribute(&vertex_attribute_data[0], 0, triangle_coords_stride, VERTEX_ATTRIBUTE_TYPE_FLOAT, VERTEX_ATTRIBUTE_SIZE_2, 0, 1);
        set_vertex_attribute(&vertex_attribute_data[1], 0, triangle_positions_stride, VERTEX_ATTRIBUTE_TYPE_FLOAT, VERTEX_ATTRIBUTE_SIZE_3, 0, 1);

        orch_write_vertex_attribute_data(&orch, framebuffer_id, vertex_attribute_data);
        
        orch_draw_range(&orch, framebuffer_id, shader_id2, mode, 0, 3, 3, index);
        */
    }

    // orch_readnpixels(&orch, framebuffer_id_0, 0, 0, WIDTH, HEIGHT, TEX_RGBA8, host_colorbuffers);
    orch_readnpixels(&orch, framebuffer_id_1, 0, 0, WIDTH, HEIGHT, TEX_RGBA8, host_colorbuffers + sizeof(rgba8_t[WIDTH][HEIGHT]));
    
    auto end = std::chrono::high_resolution_clock::now();
    double total_time_microseconds = (double)std::chrono::duration_cast<std::chrono::microseconds>(end-begin).count();
  
    printf("INFO: samples=%d.\n", samples);
    printf("PERF: total_time_milliseconds=%.3fms.\n", total_time_microseconds/(1000));

    print_ppm("image.ppm", WIDTH, HEIGHT * 2, (uint8_t*)host_colorbuffers);
    
    free(host_colorbuffers);

    orch_destroy(&orch);

    return 0;
}