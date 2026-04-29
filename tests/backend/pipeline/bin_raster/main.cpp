#include <time.h>
#include <random>

#include <types.device.h>
#include "../../common.h"

#define GLSC2_PATH "../../../../glsc2"

cl_context_wrapper wrapper;
cl_program program;
cl_kernel kernel;

// kernel arguments and buffers
cl_mem a_bin_counter;
cl_mem a_num_bin_segs;
cl_mem a_num_subtris;

cl_mem g_bin_first_seg;
cl_mem g_bin_seg_count;
cl_mem g_bin_seg_data;
cl_mem g_bin_seg_next;
cl_mem g_bin_total;
cl_mem g_tri_header;
cl_mem g_tri_subtris;

#ifdef DEVICE_IMAGE_ENABLED
cl_mem t_tri_header;
#endif

cl_int c_bin_batch_sz;
cl_int c_height_bins;
cl_int c_max_bin_segs;
cl_int c_max_subtris;
cl_int c_num_bins;
cl_int c_num_tris;
cl_int c_viewport_height;
cl_int c_viewport_width;
cl_int c_width_bins;

// test data initialization
int32_t bin_batch_sz;
int32_t viewport_height, viewport_width;
int32_t triangles;


void initialize_constants() {
    bin_batch_sz = 512;
    srand(0); // time(NULL));
    viewport_height =  (rand() % 2047) + 1;
    viewport_width = (rand() % 2047) + 1;
    triangles = 1024;// (rand() % 1023) + 1;

    c_bin_batch_sz = bin_batch_sz;
    c_height_bins = ((viewport_height-1) / (CR_TILE_SIZE * CR_BIN_SIZE)) + 1;
    c_width_bins = ((viewport_width-1) / (CR_TILE_SIZE * CR_BIN_SIZE)) + 1;
    c_max_bin_segs = CR_MAXBINS_SIZE;
    c_max_subtris = triangles;
    c_num_bins = c_height_bins*c_width_bins; //16*16
    c_num_tris = triangles;
    c_viewport_height = viewport_height;
    c_viewport_width = viewport_width;

    printf("viewport_height=%d, viewport_width=%d\n", viewport_height, viewport_width);
    printf("c_height_bins=%d, c_width_bins=%d\n", c_height_bins, c_width_bins);
}

void setup_buffers() {
    size_t bins_size = c_num_bins; //c_width_bins * c_height_bins;
    size_t sizeof_bins = sizeof(cl_int[bins_size * CR_BIN_STREAMS_SIZE]);
    uint32_t max_number_bin_segments = CR_MAXBINS_SIZE * CR_BIN_STREAMS_SIZE * ((triangles/CR_BIN_SEG_SIZE) + 1);

    cl_int ZERO = 0;
    printf("sizeof_bins=%zu, %d, %zu\n",sizeof_bins, CR_BIN_STREAMS_SIZE, bins_size);
    a_bin_counter = CL_CHECK2(clCreateBuffer(wrapper.context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(cl_int), &ZERO, &_err));
    a_num_bin_segs = CL_CHECK2(clCreateBuffer(wrapper.context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(cl_int), &ZERO, &_err));
    a_num_subtris = CL_CHECK2(clCreateBuffer(wrapper.context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(cl_int), &triangles, &_err));

    g_bin_first_seg = CL_CHECK2(clCreateBuffer(wrapper.context, CL_MEM_READ_WRITE, sizeof_bins, NULL, &_err));
    g_bin_seg_count = CL_CHECK2(clCreateBuffer(wrapper.context, CL_MEM_READ_WRITE, sizeof(cl_int[max_number_bin_segments]), NULL, &_err));
    g_bin_seg_data = CL_CHECK2(clCreateBuffer(wrapper.context, CL_MEM_READ_WRITE, sizeof(cl_int[max_number_bin_segments][CR_BIN_SEG_SIZE]), NULL, &_err));
    g_bin_seg_next = CL_CHECK2(clCreateBuffer(wrapper.context, CL_MEM_READ_WRITE, sizeof(cl_int[max_number_bin_segments]), NULL, &_err));
    g_bin_total = CL_CHECK2(clCreateBuffer(wrapper.context, CL_MEM_READ_WRITE, sizeof_bins, NULL, &_err));


    triangle_header_t* tri_header = (triangle_header_t*)malloc(sizeof(triangle_header_t[triangles]));
    cl_uchar* tri_subtris = (cl_uchar*)malloc(sizeof(cl_uchar[triangles]));
    for(int i=0; i<triangles; ++i) {
        tri_header[i].v0x = rand() % (viewport_width * CR_SUBPIXEL_SIZE);
        tri_header[i].v0y = rand() % (viewport_height * CR_SUBPIXEL_SIZE);
        tri_header[i].v1x = rand() % (viewport_width * CR_SUBPIXEL_SIZE);
        tri_header[i].v1y = rand() % (viewport_height * CR_SUBPIXEL_SIZE);
        tri_header[i].v2x = rand() % (viewport_width * CR_SUBPIXEL_SIZE);
        tri_header[i].v2y = rand() % (viewport_height * CR_SUBPIXEL_SIZE);
        tri_header[i].misc = triangle_header_misc_t{0};
        tri_subtris[i] = 1; // one subtriangle per triangle
    }
    g_tri_header = CL_CHECK2(clCreateBuffer(wrapper.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(triangle_header_t[c_num_tris]), tri_header, &_err));
    g_tri_subtris = CL_CHECK2(clCreateBuffer(wrapper.context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(cl_uchar[c_num_tris]), tri_subtris, &_err));

    #ifdef DEVICE_IMAGE_ENABLED
    t_tri_header = CL_CHECK2(clCreateBuffer(wrapper.context, CL_MEM_READ_ONLY, sizeof(cl_uchar[12* c_num_tris]), NULL, &_err));
    #endif

    uint32_t count = 0;
    CL_CHECK(clSetKernelArg(kernel, count++, sizeof(a_bin_counter), &a_bin_counter));
    CL_CHECK(clSetKernelArg(kernel, count++, sizeof(a_num_bin_segs), &a_num_bin_segs));
    CL_CHECK(clSetKernelArg(kernel, count++, sizeof(a_num_subtris), &a_num_subtris));
    CL_CHECK(clSetKernelArg(kernel, count++, sizeof(g_bin_first_seg), &g_bin_first_seg));
    CL_CHECK(clSetKernelArg(kernel, count++, sizeof(g_bin_seg_count), &g_bin_seg_count));
    CL_CHECK(clSetKernelArg(kernel, count++, sizeof(g_bin_seg_data), &g_bin_seg_data));
    CL_CHECK(clSetKernelArg(kernel, count++, sizeof(g_bin_seg_next), &g_bin_seg_next));
    CL_CHECK(clSetKernelArg(kernel, count++, sizeof(g_bin_total), &g_bin_total));
    CL_CHECK(clSetKernelArg(kernel, count++, sizeof(g_tri_header), &g_tri_header));
    CL_CHECK(clSetKernelArg(kernel, count++, sizeof(g_tri_subtris), &g_tri_subtris));
    #ifdef DEVICE_IMAGE_ENABLED
    CL_CHECK(clSetKernelArg(kernel, count++, sizeof(t_tri_header), &t_tri_header));
    #endif
    CL_CHECK(clSetKernelArg(kernel, count++, sizeof(c_bin_batch_sz), &c_bin_batch_sz));
    CL_CHECK(clSetKernelArg(kernel, count++, sizeof(c_bin_batch_sz), &c_height_bins));
    CL_CHECK(clSetKernelArg(kernel, count++, sizeof(c_max_bin_segs), &c_max_bin_segs));
    CL_CHECK(clSetKernelArg(kernel, count++, sizeof(c_max_subtris), &c_max_subtris));
    CL_CHECK(clSetKernelArg(kernel, count++, sizeof(c_num_bins), &c_num_bins));
    CL_CHECK(clSetKernelArg(kernel, count++, sizeof(c_num_tris), &c_num_tris));
    CL_CHECK(clSetKernelArg(kernel, count++, sizeof(c_viewport_height), &c_viewport_height));
    CL_CHECK(clSetKernelArg(kernel, count++, sizeof(c_viewport_width), &c_viewport_width));
    CL_CHECK(clSetKernelArg(kernel, count++, sizeof(c_width_bins), &c_width_bins));
}

#define EXTERN_KERNEL(_NAME) \
    extern unsigned char _NAME##_o[]; \
    extern unsigned int _NAME##_o_len;

EXTERN_KERNEL(bin_raster);

int main() {
    wrapper = cl_context_factory();
    size_t bin_raster_size = bin_raster_o_len;
    const unsigned char *bin_raster_bin = bin_raster_o;
    program = CL_CHECK2(clCreateProgramWithBinary(wrapper.context, 1, &wrapper.device_id, &bin_raster_size, &bin_raster_bin, NULL, &_err));
    CL_CHECK(clBuildProgram(program, 1, &wrapper.device_id, NULL, NULL, NULL));

    kernel = CL_CHECK2(clCreateKernel(program, "bin_raster", &_err));

    initialize_constants();
    setup_buffers();

    cl_command_queue queue = CL_CHECK2(clCreateCommandQueue(wrapper.context, wrapper.device_id, 0, &_err));
    size_t lw_size[2] = {DEVICE_SUB_GROUP_THREADS, DEVICE_BIN_SUB_GROUPS};
    size_t gw_size[2] = {lw_size[0]*CR_BIN_STREAMS_SIZE, lw_size[1]};

    CL_CHECK(clEnqueueNDRangeKernel(queue, kernel, 2, NULL, gw_size, lw_size, 0, NULL, NULL));
    CL_CHECK(clFinish(queue));
    
    cl_int num_bin_segs, bin_counter;
    CL_CHECK(clEnqueueReadBuffer(queue, a_num_bin_segs, CL_TRUE, 0, sizeof(num_bin_segs), &num_bin_segs, 0, NULL, NULL));
    CL_CHECK(clEnqueueReadBuffer(queue, a_bin_counter, CL_TRUE, 0, sizeof(bin_counter), &bin_counter, 0, NULL, NULL));
    printf("num_bin_segs=%d, bin_counter=%d\n", num_bin_segs, bin_counter);

    size_t sizeof_bins = sizeof(cl_int[c_num_bins*CR_BIN_STREAMS_SIZE]);
    cl_int *bin_total = (cl_int*)malloc(sizeof_bins);
    CL_CHECK(clEnqueueReadBuffer(queue, g_bin_total, CL_TRUE, 0, sizeof_bins, bin_total, 0, NULL, NULL));
    for(int bin_idx=0; bin_idx < c_num_bins; ++bin_idx)
    {
        printf("bin[%d]=[", bin_idx);
        for(int stream_idx=0; stream_idx < CR_BIN_STREAMS_SIZE; ++stream_idx)
        {
            printf("%d,", bin_total[stream_idx + bin_idx*CR_BIN_STREAMS_SIZE]);
        }
        printf("]\n");
    }

}