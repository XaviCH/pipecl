#include <algorithm>
#include <cmath>
#include <chrono>
#include <vector>

#include <glm/glm.hpp>

#include <kernels/coarse_raster.ocl.c>
#include <tests/glsc2/common.h>
#include <tests/glsc2/debug.hpp>
#include <tests/glsc2/kernels/common.hpp>

// TEST DEFAULT PARAMETERS
char KERNEL_NAME[] = "coarse_raster";
size_t BINARY_SIZE = sizeof(coarse_raster_ocl);
const unsigned char* BINARY = coarse_raster_ocl;

int NUM_SUBTRIS = 0; 
uint8_t TRI_SUBTRIS[] = {0, 1};
CRTriangleHeader TRI_HEADER[] = { 
    CRTriangleHeader(), {
        800, -800, 0, 800, -800, -800, 2147483219
    }};

int INDEX_BUFFER[] = {0,1,2,2,1,0};
float VERTEX_BUFFER[] = { 
    -1, -1, 0, 1, 1, 1, 1, 0,
    0, 1, 0, 1, 1, 1, 1, 0,
    1, -1, 0, 1, 1, 1, 1, 0,
};
int NUM_TRIS = sizeof(INDEX_BUFFER)/sizeof(int)/3;

int MAX_SUBTRIS = 1;
int MAX_BIN_SEGS = 1;
int WIDTH = 100;
int HEIGHT = 100;

int NUM_BIN_SEGS;

int BIN_FIRST_SEG[] = {};
int BIN_SEG_DATA[] = {};
int BIN_SEG_NEXT[] = {};
int BIN_SEG_COUNT[] = {};

typedef struct {
    float position[4];
    float color[4];
} VertexData;

int num_samples = 1;

// UTILS

glm::ivec2 size = glm::ivec2(WIDTH, HEIGHT);
glm::ivec2 size_pixels = (size + CR_TILE_SIZE - 1) & -CR_TILE_SIZE;
glm::ivec2 size_tiles = size_pixels >> CR_TILE_LOG2;
int32_t num_tiles = size_tiles.x * size_tiles.y;
glm::ivec2 size_bins = (size_tiles + CR_BIN_SIZE -1) >> CR_BIN_LOG2;
int32_t num_bins = size_bins.x * size_bins.y;
glm::ivec2 viewport_size = glm::ivec2(WIDTH, HEIGHT);
int roundSize  = CR_BIN_WARPS * 32;
int minBatches = CR_BIN_STREAMS_SIZE * 2;
int maxRounds  = 32;

// Nvidia defs
int maxSubtrisSlack     = 4096;     // x 81B    = 324KB
int maxBinSegsSlack     = 256;      // x 2137B  = 534KB
int maxTileSegsSlack    = 4096;     // x 136B   = 544KB


// KERNEL INPUTS
cl_mem t_tri_header;

cl_mem c_bin_first_seg;
cl_mem c_bin_seg_data;
cl_mem c_bin_seg_next;
cl_mem c_bin_seg_count;
cl_mem c_num_bin_segs;
cl_mem c_num_subtris;
cl_mem c_tri_header;

cl_int c_deferred_clear;
cl_int c_height_tiles = size_tiles.y;
cl_int c_max_bin_segs = std::max(MAX_BIN_SEGS, std::max(num_bins * CR_BIN_STREAMS_SIZE, (NUM_TRIS - 1) / CR_BIN_SEG_SIZE + 1) + maxBinSegsSlack);
cl_int c_max_subtris = std::max(MAX_SUBTRIS, NUM_TRIS + maxSubtrisSlack);
cl_int c_max_tile_segs;
cl_int c_num_bins = size_bins.x * size_bins.y;
cl_int c_viewport_height = HEIGHT;
cl_int c_viewport_width = WIDTH;
cl_int c_width_bins = size_bins.x;
cl_int c_width_tiles = size_tiles.x;

// OUTPUTS
typedef struct {
    cl_int a_coarse_counter;
    cl_int a_num_active_tiles;
    cl_int a_num_tile_segs;
    cl_int *g_active_tiles;
    cl_int *g_tile_first_seg;
    cl_int *g_tile_seg_data;
    cl_int *g_tile_seg_next;
    cl_int *g_tile_seg_count;
} result_t;

result_t kernel_result, emulate_result;



// CL GLOBALS
cl_int error;
cl_platform_id platform_id;
cl_device_id device_id;
cl_context context;
cl_program program;
cl_kernel kernel;

void emulateCoarseRaster();

int main(int argc, char** argv) {

    CL_CHECK(clGetPlatformIDs(1, &platform_id, NULL));
    CL_CHECK(clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_DEFAULT, 1, &device_id, NULL));
    context = CL_CHECK2(clCreateContext(NULL, 1, &device_id, NULL, NULL,  &_err));
    program = CL_CHECK2(clCreateProgramWithBinary(context, 1, &device_id, &BINARY_SIZE, &BINARY, NULL, &_err));
    CL_CHECK(clBuildProgram(program, 1, &device_id, NULL, NULL, NULL));
    kernel = CL_CHECK2(clCreateKernel(program, KERNEL_NAME, &_err));

    // Set Up Kernel Mem
    c_bin_first_seg = CL_CHECK2(clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(BIN_FIRST_SEG), &BIN_FIRST_SEG, &_err)),    
    c_bin_seg_data  = CL_CHECK2(clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(BIN_SEG_DATA), &BIN_SEG_DATA, &_err)), 
    c_bin_seg_next  = CL_CHECK2(clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(BIN_SEG_NEXT), &BIN_SEG_NEXT, &_err)), 
    c_bin_seg_count = CL_CHECK2(clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(BIN_SEG_COUNT), &BIN_SEG_COUNT, &_err)), 
    c_num_bin_segs  = CL_CHECK2(clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(NUM_BIN_SEGS), &NUM_BIN_SEGS, &_err)), 
    c_num_subtris   = CL_CHECK2(clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(NUM_SUBTRIS), &NUM_SUBTRIS, &_err)), 
    c_tri_header    = CL_CHECK2(clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(TRI_HEADER), &TRI_HEADER, &_err));

    cl_int ZERO_32 = 0;
    cl_mem a_coarse_counter     = CL_CHECK2(clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(ZERO_32), &ZERO_32, &_err));
    cl_mem a_num_active_tiles   = CL_CHECK2(clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(ZERO_32), &ZERO_32, &_err));
    cl_mem a_num_tile_segs      = CL_CHECK2(clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(ZERO_32), &ZERO_32, &_err));
    
    cl_mem g_active_tiles       = CL_CHECK2(clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(cl_int[CR_MAXTILES_SQR]), NULL, &_err));
    cl_mem g_tile_first_seg     = CL_CHECK2(clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(cl_int[CR_MAXTILES_SQR]), NULL, &_err));
    cl_mem g_tile_seg_data      = CL_CHECK2(clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(cl_int[c_max_tile_segs * CR_TILE_SEG_SIZE]), NULL, &_err));
    cl_mem g_tile_seg_count     = CL_CHECK2(clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(cl_int[c_max_tile_segs]), NULL, &_err));
    cl_mem g_tile_seg_next      = CL_CHECK2(clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(cl_int[c_max_tile_segs]), NULL, &_err));
    
    // CONF_IMAGE_SUPPORT
    cl_image_format image_format = {
        .image_channel_order = CL_RGBA,
        .image_channel_data_type = CL_UNSIGNED_INT32,
    };
    cl_image_desc image_desc = {
        .image_type = CL_MEM_OBJECT_IMAGE1D_BUFFER,
        .image_width = sizeof(TRI_HEADER) / sizeof(TRI_HEADER[0]),
        .image_row_pitch = 0, // check this
        .mem_object = c_tri_header, 
    };
    t_tri_header            = CL_CHECK2(clCreateImage(context, CL_MEM_READ_ONLY, &image_format, &image_desc, NULL, &_err));

    CL_CHECK(clSetKernelArg(kernel, 0, sizeof(c_bin_first_seg),    &c_bin_first_seg));
    CL_CHECK(clSetKernelArg(kernel, 1, sizeof(c_bin_seg_data),   &c_bin_seg_data));
    CL_CHECK(clSetKernelArg(kernel, 2, sizeof(c_bin_seg_next),    &c_bin_seg_next));
    CL_CHECK(clSetKernelArg(kernel, 3, sizeof(c_bin_seg_count),   &c_bin_seg_count));
    CL_CHECK(clSetKernelArg(kernel, 4, sizeof(c_num_bin_segs),   &c_num_bin_segs));
    CL_CHECK(clSetKernelArg(kernel, 5, sizeof(c_num_subtris),   &c_num_subtris));
    CL_CHECK(clSetKernelArg(kernel, 6, sizeof(c_tri_header),   &c_tri_header));
    
    CL_CHECK(clSetKernelArg(kernel, 7, sizeof(a_coarse_counter),   &a_coarse_counter));
    CL_CHECK(clSetKernelArg(kernel, 8, sizeof(a_num_active_tiles),  &a_num_active_tiles));
    CL_CHECK(clSetKernelArg(kernel, 9, sizeof(a_num_tile_segs),  &a_num_tile_segs));

    CL_CHECK(clSetKernelArg(kernel, 10, sizeof(g_active_tiles), &g_active_tiles));
    CL_CHECK(clSetKernelArg(kernel, 11, sizeof(g_tile_first_seg),  &g_tile_first_seg));
    CL_CHECK(clSetKernelArg(kernel, 12, sizeof(g_tile_seg_data),  &g_tile_seg_data));
    CL_CHECK(clSetKernelArg(kernel, 13, sizeof(g_tile_seg_count), &g_tile_seg_count));
    CL_CHECK(clSetKernelArg(kernel, 14, sizeof(g_tile_seg_next),    &g_tile_seg_next));

    CL_CHECK(clSetKernelArg(kernel, 15, sizeof(c_deferred_clear),     &c_deferred_clear));
    CL_CHECK(clSetKernelArg(kernel, 16, sizeof(c_height_tiles),      &c_height_tiles));
    CL_CHECK(clSetKernelArg(kernel, 17, sizeof(c_max_bin_segs),     &c_max_bin_segs));
    CL_CHECK(clSetKernelArg(kernel, 18, sizeof(c_max_subtris),      &c_max_subtris));
    CL_CHECK(clSetKernelArg(kernel, 19, sizeof(c_max_tile_segs),         &c_max_tile_segs));
    CL_CHECK(clSetKernelArg(kernel, 20, sizeof(c_num_bins),         &c_num_bins));
    CL_CHECK(clSetKernelArg(kernel, 21, sizeof(c_viewport_height),   &c_viewport_height));
    CL_CHECK(clSetKernelArg(kernel, 22, sizeof(c_viewport_width),  &c_viewport_width));
    CL_CHECK(clSetKernelArg(kernel, 23, sizeof(c_width_bins),       &c_width_bins));
    CL_CHECK(clSetKernelArg(kernel, 24, sizeof(c_width_tiles),       &c_width_tiles));

    CL_CHECK(clSetKernelArg(kernel, 25, sizeof(t_tri_header),       &t_tri_header));

    // Enqueue Kernel
    cl_command_queue command_queue = CL_CHECK2(clCreateCommandQueue(context, device_id, NULL, &_err));
    
    glm::ivec2 block_size(32, CR_BIN_WARPS);
    glm::ivec2 size_threads = glm::ivec2(CR_BIN_STREAMS_SIZE, 1) * block_size;
    glm::ivec2 grid_size = (size_threads + block_size - 1) / block_size;
    size_t local_work_size[] = {block_size.x, block_size.y};
    size_t global_work_size[] = {grid_size.x, grid_size.y};
    // cuda to opencl
    global_work_size[0] *= local_work_size[0];
    global_work_size[1] *= local_work_size[1];

    auto begin = std::chrono::high_resolution_clock::now();
    CL_CHECK(clEnqueueNDRangeKernel(command_queue, kernel, 2, NULL, global_work_size, local_work_size, 0, NULL, NULL));
    CL_CHECK(clFinish(command_queue));
    auto end = std::chrono::high_resolution_clock::now(); 
    
    // Get Kernel Resutls
    kernel_result.g_active_tiles    = (cl_int*) malloc(sizeof(cl_int[CR_MAXTILES_SQR]));
    kernel_result.g_tile_first_seg  = (cl_int*) malloc(sizeof(cl_int[CR_MAXTILES_SQR]));
    kernel_result.g_tile_seg_data   = (cl_int*) malloc(sizeof(cl_int[c_max_tile_segs][CR_TILE_SEG_SIZE]));
    kernel_result.g_tile_seg_count  = (cl_int*) malloc(sizeof(cl_int[c_max_tile_segs]));
    kernel_result.g_tile_seg_next   = (cl_int*) malloc(sizeof(cl_int[c_max_tile_segs]));

    CL_CHECK(clEnqueueReadBuffer(command_queue, a_coarse_counter,   CL_TRUE, 0, sizeof(cl_int), &kernel_result.a_coarse_counter,    0, NULL, NULL));
    CL_CHECK(clEnqueueReadBuffer(command_queue, a_num_active_tiles, CL_TRUE, 0, sizeof(cl_int), &kernel_result.a_num_active_tiles,  0, NULL, NULL));
    CL_CHECK(clEnqueueReadBuffer(command_queue, a_num_tile_segs,    CL_TRUE, 0, sizeof(cl_int), &kernel_result.a_num_tile_segs,     0, NULL, NULL));

    CL_CHECK(clEnqueueReadBuffer(command_queue, g_active_tiles,     CL_TRUE, 0, sizeof(cl_int[CR_MAXTILES_SQR]),                    kernel_result.g_active_tiles,  0, NULL, NULL));
    CL_CHECK(clEnqueueReadBuffer(command_queue, g_tile_first_seg,   CL_TRUE, 0, sizeof(cl_int[CR_MAXTILES_SQR]),                    kernel_result.g_tile_first_seg,   0, NULL, NULL));
    CL_CHECK(clEnqueueReadBuffer(command_queue, g_tile_seg_data,    CL_TRUE, 0, sizeof(cl_int[c_max_tile_segs][CR_TILE_SEG_SIZE]),  kernel_result.g_tile_seg_data,   0, NULL, NULL));
    CL_CHECK(clEnqueueReadBuffer(command_queue, g_tile_seg_count,   CL_TRUE, 0, sizeof(cl_int[c_max_tile_segs]),                    kernel_result.g_tile_seg_count,  0, NULL, NULL));
    CL_CHECK(clEnqueueReadBuffer(command_queue, g_tile_seg_next,    CL_TRUE, 0, sizeof(cl_int[c_max_tile_segs]),                    kernel_result.g_tile_seg_next,      0, NULL, NULL));

    emulateCoarseRaster();
    
    printf("INFO: kernel time = %d ns\n", std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count());
    
    // ASSERT_EQ_I(emulate_result.a_bin_counter, kernel_result.a_bin_counter);
    ASSERT_EQ_I(emulate_result.a_coarse_counter, kernel_result.a_coarse_counter);
    ASSERT_EQ_I(emulate_result.a_num_active_tiles, kernel_result.a_num_active_tiles);
    ASSERT_EQ_I(emulate_result.a_num_tile_segs, kernel_result.a_num_tile_segs);
    for(int i=0; i<emulate_result.a_coarse_counter; ++i) {
        ASSERT_EQ_I(emulate_result.g_active_tiles[i], kernel_result.g_active_tiles[i]);
        ASSERT_EQ_I(emulate_result.g_tile_first_seg[i], kernel_result.g_tile_first_seg[i]);
    }
    for(int i=0; i<emulate_result.a_num_tile_segs; ++i) {
        ASSERT_EQ_I(emulate_result.g_tile_seg_count[i], kernel_result.g_tile_seg_count[i]);
        ASSERT_EQ_I(emulate_result.g_tile_seg_next[i], kernel_result.g_tile_seg_next[i]);
    } 

    printf("TEST PASS.\n");
}

void emulateCoarseRaster()
{
    // Initialize.

    const CRTriangleHeader* triHeader       = TRI_HEADER;

    // const S32*              binSegData      = (const S32*)m_binSegData.getPtr();
    // const S32*              binSegNext      = (const S32*)m_binSegNext.getPtr();
    // const S32*              binSegCount     = (const S32*)m_binSegCount.getPtr();
    // CRAtomics&              atomics         = *(CRAtomics*)m_module->getGlobal("g_crAtomics").getMutablePtr();

    emulate_result.g_active_tiles    = (cl_int*) malloc(sizeof(cl_int[CR_MAXTILES_SQR]));
    emulate_result.g_tile_first_seg  = (cl_int*) malloc(sizeof(cl_int[CR_MAXTILES_SQR]));
    emulate_result.g_tile_seg_data   = (cl_int*) malloc(sizeof(cl_int[c_max_tile_segs][CR_TILE_SEG_SIZE]));
    emulate_result.g_tile_seg_count  = (cl_int*) malloc(sizeof(cl_int[c_max_tile_segs]));
    emulate_result.g_tile_seg_next   = (cl_int*) malloc(sizeof(cl_int[c_max_tile_segs]));

    std::vector<int> mergedTris;
    std::vector<int> currSeg(num_tiles, NULL);
    std::vector<int> idxInSeg(num_tiles, NULL);

    if (NUM_SUBTRIS > MAX_SUBTRIS || NUM_BIN_SEGS > MAX_BIN_SEGS)
        return;

    for (int i = 0; i < num_tiles; i++)
    {
        emulate_result.g_tile_first_seg[i] = -1;
        currSeg[i] = -1;
        idxInSeg[i] = CR_TILE_SEG_SIZE;
    }

    // Process each bin.

    for (int binIdx = 0; binIdx < num_bins; binIdx++)
    {
        int binTileX = (binIdx % size_bins.x) * CR_BIN_SIZE;
        int binTileY = (binIdx / size_bins.x) * CR_BIN_SIZE;

        // Merge streams.

        mergedTris.clear();
        int streamSeg[CR_BIN_STREAMS_SIZE];
        for (int i = 0; i < CR_BIN_STREAMS_SIZE; i++)
            streamSeg[i] = BIN_FIRST_SEG[binIdx * CR_BIN_STREAMS_SIZE + i];

        for (;;)
        {
            // Pick the stream with the lowest triangle index.
            const uint64_t S64_MAX = ~((long)-1 << 63);
            
            int64_t smin = S64_MAX;
            for (int i = 0; i < CR_BIN_STREAMS_SIZE; i++)
                if (streamSeg[i] != -1)
                    smin = std::min(smin, ((int64_t)BIN_SEG_DATA[streamSeg[i] * CR_BIN_SEG_SIZE] << 32) | i);
            if (smin == S64_MAX)
                break;

            // Consume one segment from the stream.

            int segIdx = streamSeg[(int32_t)smin];
            streamSeg[(int32_t)smin] = BIN_SEG_NEXT[segIdx];
            for (int i = 0; i < BIN_SEG_COUNT[segIdx]; i++)
				mergedTris.push_back(BIN_SEG_DATA[segIdx * CR_BIN_SEG_SIZE + i]);
        }

        // Rasterize each triangle into tiles.

        for (int mergedIdx = 0; mergedIdx < mergedTris.size(); mergedIdx++)
        {
            int triIdx = mergedTris[mergedIdx];
            int dataIdx = triIdx >> 3;
            int subtriIdx = triIdx & 7;
            if (subtriIdx != 7)
                dataIdx = triHeader[dataIdx].misc + subtriIdx;

            // Read vertices and compute AABB.

            const CRTriangleHeader& tri = triHeader[dataIdx];
            glm::ivec2 v0 = glm::ivec2(tri.v0x, tri.v0y);
            glm::ivec2 d01 = glm::ivec2(tri.v1x, tri.v1y) - v0;
            glm::ivec2 d02 = glm::ivec2(tri.v2x, tri.v2y) - v0;
            v0 += viewport_size * CR_SUBPIXEL_SIZE / 2;
            glm::ivec2 lo = v0 + glm::min(glm::ivec2(0), glm::min(d01, d02));
            glm::ivec2 hi = v0 + glm::max(glm::ivec2(0), glm::max(d01, d02));

            // Check against each tile.

            for (int tileInBin = 0; tileInBin < CR_BIN_SQR; tileInBin++)
            {
                int tileX = tileInBin % CR_BIN_SIZE + binTileX;
                int tileY = tileInBin / CR_BIN_SIZE + binTileY;
                int half = CR_TILE_SIZE * CR_SUBPIXEL_SIZE / 2;
                glm::ivec2 center = (glm::ivec2(tileX, tileY) * 2 + 1) * half;

                // Outside viewport => skip.

                if (tileX >= size_tiles.x || tileY >= size_tiles.y)
                    continue;

                // No intersection => skip.

                if (lo.x >= center.x + half || lo.y >= center.y + half || hi.x <= center.x - half || hi.y <= center.y - half)
                    continue;

                glm::ivec2 p0 = center - v0;
                glm::ivec2 p1 = p0 - d01;
                glm::ivec2 d12 = d02 - d01;
                if ((int64_t)p0.x * d01.y - (int64_t)p0.y * d01.x >= (abs(d01.x) + abs(d01.y)) * half) continue;
                if ((int64_t)p0.y * d02.x - (int64_t)p0.x * d02.y >= (abs(d02.x) + abs(d02.y)) * half) continue;
                if ((int64_t)p1.x * d12.y - (int64_t)p1.y * d12.x >= (abs(d12.x) + abs(d12.y)) * half) continue;

                // Segment full => allocate a new one.

                int si = tileX + tileY * size_tiles.x;
                if (idxInSeg[si] == CR_TILE_SEG_SIZE)
                {
                    int segIdx = std::min(emulate_result.a_num_active_tiles++, c_max_tile_segs - 1);
                    if (currSeg[si] == -1)
                        emulate_result.g_tile_first_seg[si] = segIdx;
                    else
                        emulate_result.g_tile_seg_next[currSeg[si]] = segIdx;

                    emulate_result.g_tile_seg_next[segIdx] = -1;
                    emulate_result.g_tile_seg_count[segIdx] = CR_TILE_SEG_SIZE;
                    currSeg[si] = segIdx;
                    idxInSeg[si] = 0;
                }

                // Append to the current segment.

                emulate_result.g_tile_seg_data[currSeg[si] * CR_TILE_SEG_SIZE + idxInSeg[si]] = triIdx;
                idxInSeg[si]++;
            }
        }
    }

    // Flush.

    for (int i = 0; i < num_tiles; i++)
    {
        if (currSeg[i] == -1 && c_deferred_clear)
            emulate_result.g_tile_first_seg[i] = -1;
        if (currSeg[i] != -1 || c_deferred_clear)
            emulate_result.g_active_tiles[emulate_result.a_num_active_tiles++] = i;
		if (idxInSeg[i] != CR_TILE_SEG_SIZE)
			emulate_result.g_tile_seg_count[currSeg[i]] = idxInSeg[i];
    }
}