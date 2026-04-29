#include <assert.h>
#include <math.h>

#include <unistd.h> 
#include <string.h>

#include "../common.h"

#define WIDTH 1500
#define HEIGHT 1000

static float position[] = {
   0.0, 1.0, 0.0,
  -1.0,-1.0, 0.0,
   1.0,-1.0, 0.0
};
static float color[] = {
   1.0, 0.0, 0.0,
   0.0, 1.0, 0.0,
   0.0, 0.0, 1.0
};

int main() {
  // Set up vertex buffer object
  uint8_t result[WIDTH][HEIGHT][4]; // RGBA8 32 bits x fragment
  GLuint vbo, program, framebuffer, colorbuffer;
  GLint loc_position, loc_color;

  EGL_SETUP();

  // Set Up Frame Context
  glGenFramebuffers(1, &framebuffer);
  glGenTextures(1, &colorbuffer);

  glBindTexture(GL_TEXTURE_2D, colorbuffer);
  glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, WIDTH, HEIGHT);
  // glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, WIDTH, HEIGHT);
  
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorbuffer, 0);
  // glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorbuffer);

  glViewport(0, 0, WIDTH, HEIGHT); 

  // Set Up Program
  program = glCreateProgram();
  LINK_PROGRAM(program, "kernel");
  glUseProgram(program);

  // Set Up Vertex Attributes
  loc_position  = glGetAttribLocation(program, "position");
  loc_color     = glGetAttribLocation(program, "in_color");
  
  glVertexAttribPointer(loc_position, 3, GL_FLOAT, GL_FALSE, 0, &position);
  glEnableVertexAttribArray(loc_position); 

  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(color), &color, GL_STATIC_DRAW);
  glVertexAttribPointer(loc_color, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
  glEnableVertexAttribArray(loc_color); 

  // Draw
  glClear(GL_COLOR_BUFFER_BIT);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glFinish();
  
  #ifdef C_OPENGL_HOST
  glReadPixels(0,0,WIDTH, HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, result);
  #else
  glReadnPixels(0,0,WIDTH, HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, WIDTH*HEIGHT*4, result);
  #endif

  print_ppm("image.ppm", WIDTH, HEIGHT, (uint8_t*) result);

  EGL_DESTROY();

  return 0; 
}