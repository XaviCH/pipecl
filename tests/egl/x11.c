#define CL_TARGET_OPENCL_VERSION 120

#include <X11/Xlib.h>
#include <CL/cl.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <unistd.h>

#define WIDTH 800
#define HEIGHT 600
#define SIZE (WIDTH * HEIGHT * 4)

Display *display;
Window window;
GC gc;
XImage *image;
unsigned char *pixels;

/* ---------------- OpenCL ---------------- */

cl_context cl_ctx;
cl_command_queue cl_queue;
cl_kernel kernel;
cl_mem cl_buf;

const char *kernelSrc =
"__kernel void render(__global uchar4* img, float t) {"
"  int x = get_global_id(0);"
"  int y = get_global_id(1);"
"  int i = y * 800 + x;"
"  uchar r = (uchar)(127 + 127*sin(t + x*0.01));"
"  uchar g = (uchar)(127 + 127*sin(t + y*0.01));"
"  uchar b = (uchar)(127 + 127*sin(t));"
"  img[i] = (uchar4)(b,g,r,255);"
"}";

void init_opencl()
{
    cl_platform_id platform;
    cl_device_id device;

    clGetPlatformIDs(1, &platform, NULL);
    clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);

    cl_ctx = clCreateContext(NULL, 1, &device, NULL, NULL, NULL);
    cl_queue = clCreateCommandQueue(cl_ctx, device, 0, NULL);

    cl_program program =
        clCreateProgramWithSource(cl_ctx, 1, &kernelSrc, NULL, NULL);

    clBuildProgram(program, 0, NULL, NULL, NULL, NULL);

    kernel = clCreateKernel(program, "render", NULL);

    cl_buf = clCreateBuffer(cl_ctx,
                            CL_MEM_WRITE_ONLY,
                            SIZE,
                            NULL,
                            NULL);
}

/* ---------------- X11 ---------------- */

void init_x11()
{
    display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Cannot open X display\n");
        exit(1);
    }

    int screen = DefaultScreen(display);

    window = XCreateSimpleWindow(display,
                                  RootWindow(display, screen),
                                  0, 0,
                                  WIDTH, HEIGHT,
                                  1,
                                  BlackPixel(display, screen),
                                  WhitePixel(display, screen));

    XSelectInput(display, window, ExposureMask | KeyPressMask);
    XMapWindow(display, window);

    gc = DefaultGC(display, screen);

    pixels = malloc(SIZE);

    image = XCreateImage(display,
                         DefaultVisual(display, screen),
                         24,
                         ZPixmap,
                         0,
                         (char*)pixels,
                         WIDTH,
                         HEIGHT,
                         32,
                         0);
}

/* ---------------- Render ---------------- */

void draw(float t)
{
    size_t global[2] = { WIDTH, HEIGHT };

    clSetKernelArg(kernel, 0, sizeof(cl_mem), &cl_buf);
    clSetKernelArg(kernel, 1, sizeof(float), &t);

    clEnqueueNDRangeKernel(cl_queue,
                           kernel,
                           2,
                           NULL,
                           global,
                           NULL,
                           0,
                           NULL,
                           NULL);

    clFinish(cl_queue);

    clEnqueueReadBuffer(cl_queue,
                        cl_buf,
                        CL_TRUE,
                        0,
                        SIZE,
                        pixels,
                        0,
                        NULL,
                        NULL);

    XPutImage(display,
              window,
              gc,
              image,
              0, 0,
              0, 0,
              WIDTH, HEIGHT);

    XFlush(display);
}

/* ---------------- Main ---------------- */

int main()
{
    init_x11();
    init_opencl();

    float t = 0;

    while (1)
    {
        while (XPending(display))
        {
            XEvent e;
            XNextEvent(display, &e);

            if (e.type == KeyPress)
                return 0;
        }

        draw(t);
        t += 0.02f;

        usleep(16000); // ~60 FPS
    }

    return 0;
}