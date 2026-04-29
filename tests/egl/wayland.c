#define CL_TARGET_OPENCL_VERSION 120

#include <wayland-client.h>
#include <CL/cl.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define WIDTH 800
#define HEIGHT 600
#define STRIDE (WIDTH * 4)
#define SIZE (STRIDE * HEIGHT)

struct wl_display *display;
struct wl_compositor *compositor;
struct wl_surface *surface;
struct wl_shm *shm;

uint32_t *shm_data;
struct wl_buffer *buffer;

/* ---------------- Shared Memory ---------------- */

static int create_shm_file(size_t size)
{
    char template[] = "/tmp/wayland-shm-XXXXXX";
    int fd = mkstemp(template);
    unlink(template);
    ftruncate(fd, size);
    return fd;
}

static void create_buffer()
{
    int fd = create_shm_file(SIZE);

    shm_data = mmap(NULL, SIZE,
                    PROT_READ | PROT_WRITE,
                    MAP_SHARED, fd, 0);

    struct wl_shm_pool *pool =
        wl_shm_create_pool(shm, fd, SIZE);

    buffer = wl_shm_pool_create_buffer(
        pool,
        0,
        WIDTH,
        HEIGHT,
        STRIDE,
        WL_SHM_FORMAT_ARGB8888);

    wl_shm_pool_destroy(pool);
    close(fd);
}

/* ---------------- OpenCL ---------------- */

cl_context clContext;
cl_command_queue clQueue;
cl_kernel clKernel;
cl_mem clBuffer;

const char *kernelSrc =
"__kernel void render(__global uchar4* img, float t) {"
"  int x = get_global_id(0);"
"  int y = get_global_id(1);"
"  int i = y * 800 + x;"
"  float fx = x / 800.0f;"
"  float fy = y / 600.0f;"
"  uchar r = (uchar)(127 + 127*sin(t + fx*5));"
"  uchar g = (uchar)(127 + 127*sin(t + fy*5));"
"  uchar b = (uchar)(127 + 127*sin(t));"
"  img[i] = (uchar4)(b,g,r,255);"
"}";

void init_opencl()
{
    cl_platform_id platform;
    cl_device_id device;

    clGetPlatformIDs(1, &platform, NULL);
    clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);

    clContext = clCreateContext(NULL, 1, &device, NULL, NULL, NULL);
    clQueue = clCreateCommandQueue(clContext, device, 0, NULL);

    cl_program program =
        clCreateProgramWithSource(clContext, 1, &kernelSrc, NULL, NULL);

    clBuildProgram(program, 0, NULL, NULL, NULL, NULL);

    clKernel = clCreateKernel(program, "render", NULL);

    clBuffer = clCreateBuffer(clContext,
                              CL_MEM_WRITE_ONLY,
                              SIZE,
                              NULL,
                              NULL);
}

/* ---------------- Wayland registry ---------------- */

static void registry_handler(void *data,
                             struct wl_registry *registry,
                             uint32_t id,
                             const char *interface,
                             uint32_t version)
{
    if (strcmp(interface, "wl_compositor") == 0)
        compositor = wl_registry_bind(registry, id,
                                       &wl_compositor_interface, 1);

    if (strcmp(interface, "wl_shm") == 0)
        shm = wl_registry_bind(registry, id,
                                &wl_shm_interface, 1);
}

static void registry_remove(void *data,
                            struct wl_registry *registry,
                            uint32_t id) {}

static const struct wl_registry_listener registry_listener = {
    registry_handler,
    registry_remove
};

/* ---------------- Render loop ---------------- */

void draw(float t)
{
    size_t global[2] = { WIDTH, HEIGHT };

    clSetKernelArg(clKernel, 0, sizeof(cl_mem), &clBuffer);
    clSetKernelArg(clKernel, 1, sizeof(float), &t);

    clEnqueueNDRangeKernel(clQueue,
                           clKernel,
                           2,
                           NULL,
                           global,
                           NULL,
                           0,
                           NULL,
                           NULL);

    clFinish(clQueue);

    clEnqueueReadBuffer(clQueue,
                        clBuffer,
                        CL_TRUE,
                        0,
                        SIZE,
                        shm_data,
                        0,
                        NULL,
                        NULL);

    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_damage(surface, 0, 0, WIDTH, HEIGHT);
    wl_surface_commit(surface);
}

/* ---------------- Main ---------------- */

int main()
{
    display = wl_display_connect(NULL);

    printf("hey %p\n", display);

    struct wl_registry *registry =
        wl_display_get_registry(display);

    printf("hey\n");

    wl_registry_add_listener(registry,
                             &registry_listener,
                             NULL);

    printf("hey\n");

    wl_display_roundtrip(display);

    printf("hey\n");
    
    surface = wl_compositor_create_surface(compositor);

    create_buffer();
    
    printf("hey\n");
    
    init_opencl();

    float t = 0;

    while (1)
    {
        draw(t);
        t += 0.02f;
        wl_display_dispatch_pending(display);
    }

    return 0;
}