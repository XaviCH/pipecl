#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <chrono>

#include <unistd.h> 
#include <string.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


#include "../common.h"

#define WIDTH 1000
#define HEIGHT 1000

int INDEX_BUFFER[] = {
    0, 1, 3, 3, 1, 2,
    1, 5, 2, 2, 5, 6,
    5, 4, 6, 6, 4, 7,
    4, 0, 7, 7, 0, 3,
    3, 2, 7, 7, 2, 6,
    4, 5, 0, 0, 5, 1,
};

float position[] = {
    -1, -1, -1,
    1, -1, -1,
    1, 1, -1, 
    -1, 1, -1, 
    -1, -1, 1, 
    1, -1, 1, 
    1, 1, 1, 
    -1, 1, 1,
};

float color[] {
  1, 0, 0,
  0, 1, 0,
  0, 0, 1,
  1, 1, 0,
  0, 1, 1,
  1, 0, 1,
  1, 1, 1,
  0, 0, 0
};

static float texCoord[] = {
  0, 0,
  0, 1,
  1, 0,
  1, 1,
  0, 1,
  0, 0,
  1, 0,
  1, 1,
};

int main() {
  // Set up vertex buffer object
  GLuint program, framebuffer, colorbuffer, depthbuffer;

  EGL_SETUP();

  // Set Up Frame Context
  glGenFramebuffers(1, &framebuffer);
  glGenRenderbuffers(1, &colorbuffer);
  glGenRenderbuffers(1, &depthbuffer);
  glGenRenderbuffers(1, &depthbuffer);

  glBindRenderbuffer(GL_RENDERBUFFER, colorbuffer);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, WIDTH, HEIGHT);

  glBindRenderbuffer(GL_RENDERBUFFER, depthbuffer);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, WIDTH, HEIGHT);

  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorbuffer);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthbuffer);

  glViewport(0, 0, WIDTH, HEIGHT);

  // Set Up Program
  program = glCreateProgram();
  LINK_PROGRAM(program, "kernel");
  glUseProgram(program);
  
  // Set Vertex Attributes
  GLint loc_position, loc_color;

  loc_position  = glGetAttribLocation(program, "position");
  loc_color     = glGetAttribLocation(program, "color");

  glVertexAttribPointer(loc_position, 3, GL_FLOAT, GL_FALSE, 0, &position);
  glEnableVertexAttribArray(loc_position);

  glVertexAttribPointer(loc_color, 3, GL_FLOAT, GL_FALSE, 0, &color);
  glEnableVertexAttribArray(loc_color);

  auto begin = std::chrono::high_resolution_clock::now();
  // Default Clearing
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  // Paint to 1 red and blue channel and half, 0.5 depth and 1 to stencil
  glClearColor(1, 1, 1, 1);
  glColorMask(GL_TRUE, GL_FALSE, GL_TRUE, GL_FALSE);

  glClearDepthf(0.5);
  
  glClearStencil(1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  // Mask all buffers and clear
  glClearColor(0, 0, 0, 0);
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

  glClearDepthf(0.5);
  glDepthMask(GL_FALSE);

  glClearStencil(2);
  glStencilMask(0);
  glFinish();

  auto end = std::chrono::high_resolution_clock::now();
  printf("PERF: clear_time=%.3fms.\n", (double)std::chrono::duration_cast<std::chrono::microseconds>(end-begin).count()/1000.0);

  uint8_t* result = (uint8_t*) malloc(sizeof(uint8_t[WIDTH][HEIGHT][4])); // RGBA8 32 bits x fragment

  #ifdef C_OPENGL_HOST
    glReadPixels(0,0,WIDTH, HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, result);
    #else
    glReadnPixels(0,0,WIDTH, HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, WIDTH*HEIGHT*4, result);
    #endif
    glFinish();
    print_ppm("image.ppm", WIDTH, HEIGHT, (uint8_t*) result);

  EGL_DESTROY();

  return 0; 
}