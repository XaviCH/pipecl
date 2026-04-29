#include <algorithm>
#include <cmath>
#include <chrono>

#include <glm/glm.hpp>

#include <kernels/triangle_setup.ocl.c>
#include <tests/glsc2/common.h>
#include <tests/glsc2/debug.hpp>
#include <tests/glsc2/kernels/common.hpp>

// TEST DEFAULT PARAMETERS
char KERNEL_NAME[] = "triangle_setup";
size_t BINARY_SIZE = sizeof(triangle_setup_ocl);
const unsigned char* BINARY = triangle_setup_ocl;
int INDEX_BUFFER[] = {0,1,2,2,1,0};
float VERTEX_BUFFER[] = { 
    -1, -1, 0, 1, 1, 1, 1, 0,
    0, 1, 0, 1, 1, 1, 1, 0,
    1, -1, 0, 1, 1, 1, 1, 0,
};
int NUM_TRIS = sizeof(INDEX_BUFFER)/sizeof(int)/3;
int maxSubtrisSlack = 4096; // TODO: Check this
int MAX_SUBTRIS = 1;
int WIDTH = 2048;
int HEIGHT = 2048;

typedef struct {
    float position[4];
    float color[4];
} VertexData;
 
int num_samples = 1;

// KERNEL INPUTS
cl_int samples_log2 = 0;
cl_uint render_mode_flags = RENDER_MODE_FLAG_ENABLE_DEPTH | RENDER_MODE_FLAG_ENABLE_LERP;
const glm::ivec2 viewport_size = {WIDTH, HEIGHT};
cl_int c_max_subtris = std::max(MAX_SUBTRIS, NUM_TRIS + maxSubtrisSlack);

// OUTPUTS
typedef struct {
    cl_int a_num_subtris;
    cl_uchar* tri_subtris;
    CRTriangleHeader* tri_header;
    CRTriangleData* tri_data;
} result_t;

result_t kernel_result, emulate_result;

// GLOBALS
cl_int error;
cl_platform_id platform_id;
cl_device_id device_id;
cl_context context;
cl_program program;
cl_kernel kernel;


void emulateTriangleSetup();

bool setupTriangle(
    int triIdx,
    const glm::vec4& v0, const glm::vec4& v1, const glm::vec4& v2,
    const glm::vec2& b0, const glm::vec2& b1, const glm::vec2& b2,
    const glm::ivec3& vidx);

int main(int argc, char** argv) {

    CL_CHECK(clGetPlatformIDs(1, &platform_id, NULL));
    CL_CHECK(clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_DEFAULT, 1, &device_id, NULL));
    context = CL_CHECK2(clCreateContext(NULL, 1, &device_id, NULL, NULL,  &_err));
    program = CL_CHECK2(clCreateProgramWithBinary(context, 1, &device_id, &BINARY_SIZE, &BINARY, NULL, &_err));
    CL_CHECK(clBuildProgram(program, 1, &device_id, NULL, NULL, NULL));
    kernel = CL_CHECK2(clCreateKernel(program, KERNEL_NAME, &_err));

    cl_mem c_index_buffer = CL_CHECK2(clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, sizeof(INDEX_BUFFER), &INDEX_BUFFER, &_err));
    cl_int c_num_tris = NUM_TRIS;
    cl_mem g_tri_header = CL_CHECK2(clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(CRTriangleHeader[c_max_subtris]), NULL, &_err));
    cl_mem g_tri_data = CL_CHECK2(clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(CRTriangleData[c_max_subtris]), NULL, &_err));
    cl_int vertex_size = sizeof(VertexData);
    cl_image_format image_format = {
        .image_channel_order = CL_RGBA,
        .image_channel_data_type = CL_FLOAT,
    };
    cl_image_desc image_desc = {
        .image_type = CL_MEM_OBJECT_IMAGE1D,
        .image_width = (vertex_size / sizeof(float[4])) * NUM_TRIS * 3,
        .image_row_pitch = vertex_size * NUM_TRIS * 3,
        // .image_slice_pitch = 0,
        // .num_mip_levels = 0,
        // .num_samples = 0,
    }; 
    cl_mem t_vertex_buffer = CL_CHECK2(clCreateImage(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &image_format, &image_desc, &VERTEX_BUFFER, &_err));
    cl_mem g_tri_subtris = CL_CHECK2(clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(cl_uchar[c_max_subtris]), NULL, &_err));
    cl_mem a_num_subtris = CL_CHECK2(clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(int), NULL, &_err)); // atomic
    cl_int c_viewport_width = WIDTH, c_viewport_height = HEIGHT;

    // cl_mem debug_return = CL_CHECK2(clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(int[3]), NULL, &_err));
    // cl_mem debug_prep_tri_value = CL_CHECK2(clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(int[3]), NULL, &_err));
    // cl_mem debug_num_verts = CL_CHECK2(clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(int[3]), NULL, &_err));
    // cl_mem debug_prep_tri_params = CL_CHECK2(clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(float[30]), NULL, &_err));
    // cl_mem debug_snap_tri_params = CL_CHECK2(clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(float[18]), NULL, &_err));
    // cl_mem debug_bary_values = CL_CHECK2(clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(float[6]), NULL, &_err));
    // cl_mem debug_vertex_values = CL_CHECK2(clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(float[9]), NULL, &_err));
    // cl_mem debug_indices_values = CL_CHECK2(clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(int[3]), NULL, &_err));

    uint32_t counter = 0;
    CL_CHECK(clSetKernelArg(kernel, counter++, sizeof(a_num_subtris),     &a_num_subtris));
    CL_CHECK(clSetKernelArg(kernel, counter++, sizeof(c_index_buffer),    &c_index_buffer));
    CL_CHECK(clSetKernelArg(kernel, counter++, sizeof(g_tri_header),      &g_tri_header));
    CL_CHECK(clSetKernelArg(kernel, counter++, sizeof(g_tri_data),        &g_tri_data));
    CL_CHECK(clSetKernelArg(kernel, counter++, sizeof(g_tri_subtris),     &g_tri_subtris));
    CL_CHECK(clSetKernelArg(kernel, counter++, sizeof(t_vertex_buffer),   &t_vertex_buffer));
    CL_CHECK(clSetKernelArg(kernel, counter++, sizeof(c_num_tris),        &c_num_tris));
    CL_CHECK(clSetKernelArg(kernel, counter++, sizeof(c_max_subtris),     &c_max_subtris));
    CL_CHECK(clSetKernelArg(kernel, counter++, sizeof(render_mode_flags),       &render_mode_flags));
    CL_CHECK(clSetKernelArg(kernel, counter++, sizeof(samples_log2),       &samples_log2));
    CL_CHECK(clSetKernelArg(kernel, counter++, sizeof(vertex_size),       &vertex_size));
    CL_CHECK(clSetKernelArg(kernel, counter++, sizeof(c_viewport_height),       &c_viewport_height));
    CL_CHECK(clSetKernelArg(kernel, counter++, sizeof(c_viewport_width),       &c_viewport_width));
    
    // CL_CHECK(clSetKernelArg(kernel, counter++, sizeof(debug_return),            &debug_return));
    // CL_CHECK(clSetKernelArg(kernel, counter++, sizeof(debug_prep_tri_value),    &debug_prep_tri_value));
    // CL_CHECK(clSetKernelArg(kernel, counter++, sizeof(debug_num_verts),    &debug_num_verts));
    // CL_CHECK(clSetKernelArg(kernel, counter++, sizeof(debug_prep_tri_params),    &debug_prep_tri_params));
    // CL_CHECK(clSetKernelArg(kernel, counter++, sizeof(debug_snap_tri_params),    &debug_snap_tri_params));
    // CL_CHECK(clSetKernelArg(kernel, counter++, sizeof(debug_bary_values),    &debug_bary_values));
    // CL_CHECK(clSetKernelArg(kernel, counter++, sizeof(debug_vertex_values),    &debug_vertex_values));
    // CL_CHECK(clSetKernelArg(kernel, counter++, sizeof(debug_indices_values),    &debug_indices_values));

    cl_command_queue command_queue = CL_CHECK2(clCreateCommandQueue(context, device_id, NULL, &_err));
    
    size_t local_work_size[] = {32, 2}; // WARPS & CR_SETUP_WARPS
    int threadsPerBlock = local_work_size[0] * local_work_size[1];
    int maxGridWidth = 65536; // ??
    size_t global_work_size[] = {(NUM_TRIS + threadsPerBlock - 1) / threadsPerBlock, 1};
    while (global_work_size[0] > maxGridWidth)
    {
        global_work_size[0] = (global_work_size[0] + 1) >> 1;
        global_work_size[1] <<= 1;
    }
    global_work_size[0] *= local_work_size[0];
    global_work_size[1] *= local_work_size[1];

    auto begin = std::chrono::high_resolution_clock::now();
    CL_CHECK(clEnqueueNDRangeKernel(command_queue, kernel, 2, NULL, global_work_size, local_work_size, 0, NULL, NULL));
    CL_CHECK(clFinish(command_queue));
    auto end = std::chrono::high_resolution_clock::now(); 
    
    kernel_result.a_num_subtris = 0;
    kernel_result.tri_subtris  = (cl_uchar*)           malloc(sizeof(cl_uchar          [c_max_subtris]));
    kernel_result.tri_header   = (CRTriangleHeader*)   malloc(sizeof(CRTriangleHeader  [c_max_subtris]));
    kernel_result.tri_data     = (CRTriangleData*)     malloc(sizeof(CRTriangleData    [c_max_subtris]));

    //int* result_debug_return = (int*) malloc(sizeof(int[3]));
    //int* result_debug_prep_tri_value = (int*) malloc(sizeof(int[3]));
    //int* result_debug_num_verts = (int*) malloc(sizeof(int[3]));
    //int* result_debug_prep_tri_params = (int*) malloc(sizeof(int[30]));
    //float* result_debug_snap_tri_params = (float*) malloc(sizeof(float[18]));
    //float* result_debug_bary_values = (float*) malloc(sizeof(float[6]));
    //float* result_debug_vertex_values = (float*) malloc(sizeof(float[9]));
    //int* result_debug_indices_values = (int*) malloc(sizeof(int[3]));

    CL_CHECK(clEnqueueReadBuffer(command_queue, a_num_subtris, CL_TRUE, 0, sizeof(cl_int), &kernel_result.a_num_subtris, 0, NULL, NULL));
    CL_CHECK(clEnqueueReadBuffer(command_queue, g_tri_subtris, CL_TRUE, 0, sizeof(cl_uchar[c_max_subtris]), kernel_result.tri_subtris, 0, NULL, NULL));
    CL_CHECK(clEnqueueReadBuffer(command_queue, g_tri_header, CL_TRUE, 0, sizeof(CRTriangleHeader[c_max_subtris]), kernel_result.tri_header, 0, NULL, NULL));
    CL_CHECK(clEnqueueReadBuffer(command_queue, g_tri_data, CL_TRUE, 0, sizeof(CRTriangleData[c_max_subtris]), kernel_result.tri_data, 0, NULL, NULL));

    // CL_CHECK(clEnqueueReadBuffer(command_queue, debug_return, CL_TRUE, 0, sizeof(int[3]), result_debug_return, 0, NULL, NULL));
    // printf("DEBUG: return return: (%d,%d,%d)\n",result_debug_return[0], result_debug_return[1], result_debug_return[2]);
    // CL_CHECK(clEnqueueReadBuffer(command_queue, debug_prep_tri_value, CL_TRUE, 0, sizeof(int[3]), result_debug_prep_tri_value, 0, NULL, NULL));
    // printf("DEBUG: return prep tri value: (%d,%d,%d)\n",result_debug_prep_tri_value[0], result_debug_prep_tri_value[1], result_debug_prep_tri_value[2]);
    // CL_CHECK(clEnqueueReadBuffer(command_queue, debug_num_verts, CL_TRUE, 0, sizeof(int[3]), result_debug_num_verts, 0, NULL, NULL));
    // printf("DEBUG: return num vert: (%d,%d,%d)\n",result_debug_num_verts[0], result_debug_num_verts[1], result_debug_num_verts[2]);
    // CL_CHECK(clEnqueueReadBuffer(command_queue, debug_prep_tri_params, CL_TRUE, 0, sizeof(int[30]), result_debug_prep_tri_params, 0, NULL, NULL));
    // printf("DEBUG: return prep tri params: (%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d)\n",
    //     result_debug_prep_tri_params[15], result_debug_prep_tri_params[16], result_debug_prep_tri_params[17],
    //     result_debug_prep_tri_params[18], result_debug_prep_tri_params[19], result_debug_prep_tri_params[20],
    //     result_debug_prep_tri_params[21], result_debug_prep_tri_params[22], result_debug_prep_tri_params[23],
    //     result_debug_prep_tri_params[24], result_debug_prep_tri_params[25], result_debug_prep_tri_params[26],
    //     result_debug_prep_tri_params[27], result_debug_prep_tri_params[28], result_debug_prep_tri_params[29]
    // );
    // CL_CHECK(clEnqueueReadBuffer(command_queue, debug_snap_tri_params, CL_TRUE, 0, sizeof(float[18]), result_debug_snap_tri_params, 0, NULL, NULL));
    // printf("DEBUG: return snap tri params: (%f,%f,%f,%f,%f,%f,%f,%f,%f)\n",
    //     result_debug_snap_tri_params[9], result_debug_snap_tri_params[10], result_debug_snap_tri_params[11],
    //     result_debug_snap_tri_params[12], result_debug_snap_tri_params[13], result_debug_snap_tri_params[14],
    //     result_debug_snap_tri_params[15], result_debug_snap_tri_params[16], result_debug_snap_tri_params[17]
    // );
// 
    // CL_CHECK(clEnqueueReadBuffer(command_queue, debug_bary_values, CL_TRUE, 0, sizeof(float[6]), result_debug_bary_values, 0, NULL, NULL));
    // printf("DEBUG: return bary values: (%f,%f,%f,%f,%f,%f)\n",
    //     result_debug_bary_values[0], result_debug_bary_values[1], result_debug_bary_values[2],
    //     result_debug_bary_values[3], result_debug_bary_values[4], result_debug_bary_values[5]
    // );
    // CL_CHECK(clEnqueueReadBuffer(command_queue, debug_vertex_values, CL_TRUE, 0, sizeof(float[9]), result_debug_vertex_values, 0, NULL, NULL));
    // printf("DEBUG: return vertex values: (%f,%f,%f,%f,%f,%f,%f,%f,%f)\n",
    //     result_debug_vertex_values[0], result_debug_vertex_values[1], result_debug_vertex_values[2],
    //     result_debug_vertex_values[3], result_debug_vertex_values[4], result_debug_vertex_values[5],
    //     result_debug_vertex_values[6], result_debug_vertex_values[7], result_debug_vertex_values[8]
    // );
    // CL_CHECK(clEnqueueReadBuffer(command_queue, debug_indices_values, CL_TRUE, 0, sizeof(int[3]), result_debug_indices_values, 0, NULL, NULL));
    // printf("DEBUG: return indices values: (%d,%d,%d)\n",result_debug_indices_values[0], result_debug_indices_values[1], result_debug_indices_values[2]);

    emulateTriangleSetup();
    
    printf("PERF: kernel time = %d ns\n", std::chrono::duration_cast<std::chrono::nanoseconds>(end-begin).count());
    
    printf("INFO: a_num_subtris = %d\n", kernel_result.a_num_subtris);
    ASSERT_EQ_I(emulate_result.a_num_subtris, kernel_result.a_num_subtris);
    for(int i=0; i<c_num_tris; ++i) { 
        printf("INFO: tri_subtris[%d] = %d\n", i, kernel_result.tri_subtris[i]);
        ASSERT_EQ_I(emulate_result.tri_subtris[i], kernel_result.tri_subtris[i]);
        if (emulate_result.tri_subtris[i]) {
            printf("INFO: tri_header[%d].v0x = %d\n", i, kernel_result.tri_header[i].v0x);
            printf("INFO: tri_header[%d].v0y = %d\n", i, kernel_result.tri_header[i].v0y);
            printf("INFO: tri_header[%d].v1x = %d\n", i, kernel_result.tri_header[i].v1x);
            printf("INFO: tri_header[%d].v1y = %d\n", i, kernel_result.tri_header[i].v1y);
            printf("INFO: tri_header[%d].v2x = %d\n", i, kernel_result.tri_header[i].v2x);
            printf("INFO: tri_header[%d].v2y = %d\n", i, kernel_result.tri_header[i].v2y);
            printf("INFO: tri_header[%d].misc = %x\n", i, kernel_result.tri_header[i].misc);
            ASSERT_EQ(emulate_result.tri_header[i], kernel_result.tri_header[i]);
            
            // if (emulate_result.tri_data[i].ub != kernel_result.tri_data[i].ub) {
            //     printf("ASSERTION: tri_data[%d].ub %d != %d\n",i, emulate_result.tri_data[i].ub, kernel_result.tri_data[i].ub);
            //     exit(1);
            // }
            ASSERT_EQ(emulate_result.tri_data[i], kernel_result.tri_data[i]);
        }
    }

    printf("TEST PASS.\n");
}

void emulateTriangleSetup()
{
    emulate_result.a_num_subtris = 0;
    emulate_result.tri_subtris  = (cl_uchar*)           malloc(sizeof(cl_uchar          [c_max_subtris]));
    emulate_result.tri_header   = (CRTriangleHeader*)   malloc(sizeof(CRTriangleHeader  [c_max_subtris]));
    emulate_result.tri_data     = (CRTriangleData*)     malloc(sizeof(CRTriangleData    [c_max_subtris]));

    for (int triIdx = 0; triIdx < NUM_TRIS; triIdx++)
    {
        const glm::ivec3 vidx = {INDEX_BUFFER[triIdx*3], INDEX_BUFFER[triIdx*3+1], INDEX_BUFFER[triIdx*3+2] }; 
        int numVerts = 3;
        bool needToClip = true;

        // Read vertices.

        glm::vec4 v[9];
        for (int i = 0; i < 3; i++){
            v[i] = {
                *(VERTEX_BUFFER + vidx[i] * sizeof(VertexData)/sizeof(float)),
                *(VERTEX_BUFFER + vidx[i] * sizeof(VertexData)/sizeof(float)+1),
                *(VERTEX_BUFFER + vidx[i] * sizeof(VertexData)/sizeof(float)+2),
                *(VERTEX_BUFFER + vidx[i] * sizeof(VertexData)/sizeof(float)+3),
                };
        }
        // Outside view frustum => cull.

        if ((v[0].x < -v[0].w && v[1].x < -v[1].w && v[2].x < -v[2].w) ||
            (v[0].x > +v[0].w && v[1].x > +v[1].w && v[2].x > +v[2].w) ||
            (v[0].y < -v[0].w && v[1].y < -v[1].w && v[2].y < -v[2].w) ||
            (v[0].y > +v[0].w && v[1].y > +v[1].w && v[2].y > +v[2].w) ||
            (v[0].z < -v[0].w && v[1].z < -v[1].w && v[2].z < -v[2].w) ||
            (v[0].z > +v[0].w && v[1].z > +v[1].w && v[2].z > +v[2].w))
        {
            numVerts = 0;
            needToClip = false;
        }

        // Within depth range => try to project.

        if (v[0].z >= -v[0].w && v[1].z >= -v[1].w && v[2].z >= -v[2].w &&
            v[0].z <= +v[0].w && v[1].z <= +v[1].w && v[2].z <= +v[2].w)
        {
            glm::vec2 viewScale = viewport_size << (CR_SUBPIXEL_LOG2 - 1);
            glm::vec2 p0 = (glm::vec2(v[0].x,v[0].y) / v[0].w) * viewScale;
            glm::vec2 p1 = (glm::vec2(v[1].x,v[1].y) / v[1].w) * viewScale;
            glm::vec2 p2 = (glm::vec2(v[2].x,v[2].y) / v[2].w) * viewScale;
            glm::vec2 lo = glm::min(p0, glm::min(p1, p2));
            glm::vec2 hi = glm::max(p0, glm::max(p1, p2));

            // Within int16_t range and small enough => no need to clip.
            // Note: aabbLimit comes from the fact that cover8x8
            // does not support guardband with maximal viewport.
            float aabbLimit = (float)((1 << (CR_MAXVIEWPORT_LOG2 + CR_SUBPIXEL_LOG2)) - 1);
            if (glm::min(lo.x, lo.y) >= -32768.5f && glm::max(hi.x, hi.y) < 32767.5f && glm::max((hi - lo).x, (hi - lo).y) <= aabbLimit) {

                needToClip = false; 
            }
        }
        // Clip if needed.

        glm::vec2 b[9];
        b[0] = glm::vec2{0.0f, 0.0f};
        b[1] = glm::vec2{1.0f, 0.0f};
        b[2] = glm::vec2{0.0f, 1.0f};

        if (needToClip)
        {
            glm::vec4 v0 = v[0];
            glm::vec4 d1 = v[1] - v[0];
            glm::vec4 d2 = v[2] - v[0];

            float bary[18];
            numVerts = clipTriangleWithFrustum(bary, &v0.x, &v[1].x, &v[2].x, &d1.x, &d2.x);

            for (int i = 0; i < numVerts; i++)
            {
                b[i] = glm::vec2(bary[i * 2 + 0], bary[i * 2 + 1]);
                v[i] = v0 + d1 * b[i].x + d2 * b[i].y;
            }
        }

        printf("numvert=%d\n",numVerts);
        // Setup subtriangles.
        int numSubtris = 0;
        for (int i = 0; i < numVerts - 2; i++)
        {
            int subtriIdx = (numSubtris == 0) ? triIdx : std::min(emulate_result.a_num_subtris + numSubtris, c_max_subtris - 1);
            if (setupTriangle(subtriIdx, v[0], v[i + 1], v[i + 2], b[0], b[i + 1], b[i + 2], vidx)) {
                numSubtris++;
            }
        }
        emulate_result.tri_subtris[triIdx] = (uint8_t)numSubtris;

        // More than one subtriangle => create indirect reference.

        if (numSubtris > 1)
        {
            if (emulate_result.a_num_subtris < c_max_subtris)
            {
                emulate_result.tri_header[emulate_result.a_num_subtris] = emulate_result.tri_header[triIdx];
                emulate_result.tri_data[emulate_result.a_num_subtris] = emulate_result.tri_data[triIdx];
            }
            emulate_result.tri_header[triIdx].misc = emulate_result.a_num_subtris;
            emulate_result.a_num_subtris += numSubtris;
        }
    }
}

bool setupTriangle(
    int triIdx,
    const glm::vec4& v0, const glm::vec4& v1, const glm::vec4& v2,
    const glm::vec2& b0, const glm::vec2& b1, const glm::vec2& b2,
    const glm::ivec3& vidx)
{
    // Snap vertices.

    glm::vec2 viewScale = glm::vec2(viewport_size << (CR_SUBPIXEL_LOG2 - 1));
    glm::vec3 rcpW = 1.0f / glm::vec3(v0.w, v1.w, v2.w);
    glm::ivec2 p0 = glm::ivec2((int32_t)std::floor(v0.x * rcpW.x * viewScale.x + 0.5f), (int32_t)std::floor(v0.y * rcpW.x * viewScale.y + 0.5f));
    glm::ivec2 p1 = glm::ivec2((int32_t)std::floor(v1.x * rcpW.y * viewScale.x + 0.5f), (int32_t)std::floor(v1.y * rcpW.y * viewScale.y + 0.5f));
    glm::ivec2 p2 = glm::ivec2((int32_t)std::floor(v2.x * rcpW.z * viewScale.x + 0.5f), (int32_t)std::floor(v2.y * rcpW.z * viewScale.y + 0.5f));
    glm::ivec2 d1 = p1 - p0;
    glm::ivec2 d2 = p2 - p0;

    // Backfacing or degenerate => cull.
    
    int32_t area = d1.x * d2.y - d1.y * d2.x;
    if (area <= 0) {  
        return false;
    } 

    // AABB falls between samples => cull.

    glm::ivec2 lo = glm::min(p0, glm::min(p1, p2));
    glm::ivec2 hi = glm::max(p0, glm::max(p1, p2));

    int sampleSize = 1 << (CR_SUBPIXEL_LOG2 - samples_log2);
    glm::ivec2 bias = glm::ivec2((viewport_size << (CR_SUBPIXEL_LOG2 - 1)) - sampleSize / 2);
    glm::ivec2 loc = (lo + bias + sampleSize - 1) & -sampleSize;
    glm::ivec2 hic = (hi + bias) & -sampleSize;

    if (loc.x > hic.x || loc.y > hic.y) {
        return false;
    }

    // AABB covers 1 or 2 samples => cull if they are not covered.

    int diff = hic.x + hic.y - loc.x - loc.y;
    if (diff <= sampleSize)
    {
        loc -= bias;
        glm::ivec2 t0 = p0 - loc;
        glm::ivec2 t1 = p1 - loc;
        glm::ivec2 t2 = p2 - loc;
        int64_t e0 = (int64_t)t0.x * t1.y - (int64_t)t0.y * t1.x;
        int64_t e1 = (int64_t)t1.x * t2.y - (int64_t)t1.y * t2.x;
        int64_t e2 = (int64_t)t2.x * t0.y - (int64_t)t2.y * t0.x;

        if (e0 < 0 || e1 < 0 || e2 < 0)
        {
            if (diff == 0) {
                return false;
            }

            hic -= bias;
            t0 = p0 - hic;
            t1 = p1 - hic;
            t2 = p2 - hic;
            e0 = (int64_t)t0.x * t1.y - (int64_t)t0.y * t1.x;
            e1 = (int64_t)t1.x * t2.y - (int64_t)t1.y * t2.x;
            e2 = (int64_t)t2.x * t0.y - (int64_t)t2.y * t0.x;

            if (e0 < 0 || e1 < 0 || e2 < 0) {
                return false;
            }
        }
    }

    // Setup plane equations.

    glm::vec3 zvert = lerp(glm::vec3(CR_DEPTH_MIN), glm::vec3(CR_DEPTH_MAX), glm::vec3(v0.z, v1.z, v2.z) * rcpW * 0.5f + 0.5f);
    glm::vec3 wvert = rcpW * (glm::min(v0.w, glm::min(v1.w, v2.w)) * (float)CR_BARY_MAX);
    glm::vec3 uvert = glm::vec3(b0.x, b1.x, b2.x) * wvert;
    glm::vec3 vvert = glm::vec3(b0.y, b1.y, b2.y) * wvert;

    glm::ivec2 wv0 = p0 + (viewport_size << (CR_SUBPIXEL_LOG2 - 1));
    glm::ivec2 zv0 = wv0 - (1 << (CR_SUBPIXEL_LOG2 - samples_log2 - 1));
    glm::ivec3 zpleq = setupPleq(zvert, zv0, d1, d2, area, samples_log2);
    glm::ivec3 wpleq = setupPleq(wvert, wv0, d1, d2, area, samples_log2 + 1);
    glm::ivec3 upleq = setupPleq(uvert, wv0, d1, d2, area, samples_log2 + 1);
    glm::ivec3 vpleq = setupPleq(vvert, wv0, d1, d2, area, samples_log2 + 1);
    uint32_t zmin = (uint32_t)std::max(
        std::floor(glm::min(zvert.x,glm::min(zvert.y,zvert.z)) + 0.5f) - CR_LERP_ERROR(samples_log2), 0.0f);
    uint32_t zslope = (uint32_t)std::min(
        ((uint64_t)abs(zpleq.x) + abs(zpleq.y)) * (num_samples / 2), (uint64_t)(uint32_t)(-1));
 
    // Write CRTriangleData.
    printf("area %f\n", area);
    printf("coef %f\n", (glm::min(v0.w, glm::min(v1.w, v2.w)) * (float)CR_BARY_MAX));
    printf("rcpW (%f,%f,%f)\n", rcpW.x, rcpW.y, rcpW.z);
    printf("wvert (%f,%f,%f)\n", wvert.x, wvert.y, wvert.z);
    printf("uvert (%f,%f,%f)\n", uvert.x, uvert.y, uvert.z);
    printf("vvert (%f,%f,%f)\n", vvert.x, vvert.y, vvert.z);
    printf("upleq (%d,%d,%d)\n", upleq.x, upleq.y, upleq.z);
    CRTriangleData& td = emulate_result.tri_data[triIdx];
    td.zx = zpleq.x, td.zy = zpleq.y, td.zb = zpleq.z; td.zslope = zslope;
    td.wx = wpleq.x, td.wy = wpleq.y, td.wb = wpleq.z;
    td.ux = upleq.x, td.uy = upleq.y, td.ub = upleq.z;
    td.vx = vpleq.x, td.vy = vpleq.y, td.vb = vpleq.z;
    td.vi0 = vidx.x, td.vi1 = vidx.y, td.vi2 = vidx.z;

    // Write CRTriangleHeader.

    CRTriangleHeader& th = emulate_result.tri_header[triIdx];
    th.v0x = (int16_t)p0.x, th.v0y = (int16_t)p0.y;
    th.v1x = (int16_t)p1.x, th.v1y = (int16_t)p1.y;
    th.v2x = (int16_t)p2.x, th.v2y = (int16_t)p2.y;
    uint32_t f01 = (uint8_t)cover8x8_selectFlips(d1.x, d1.y);
    uint32_t f12 = (uint8_t)cover8x8_selectFlips(d2.x - d1.x, d2.y - d1.y);
    uint32_t f20 = (uint8_t)cover8x8_selectFlips(-d2.x, -d2.y);
	th.misc = (zmin & 0xfffff000u) | (f01 << 6) | (f12 << 2) | (f20 >> 2);
    return true;
}