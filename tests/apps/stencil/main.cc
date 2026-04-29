#include <assert.h>
#include <math.h>

#include <unistd.h> 
#include <string.h>

#include "../common.h"

#define WIDTH 150
#define HEIGHT 100

static float triangle_position[] = {
   0.0, 1.0, 0.0,
  -1.0,-1.0, 0.0,
   1.0,-1.0, 0.0
};

static float quad_position[] = {
   1.0, 1.0, 0.0,
  -1.0,-1.0, 0.0,
   1.0,-1.0, 0.0,

   1.0, 1.0, 0.0,
  -1.0,-1.0, 0.0,
  -1.0, 1.0, 0.0,
};

int main() {
  // Set up vertex buffer object
  uint8_t result[WIDTH][HEIGHT][4]; // RGBA8 32 bits x fragment
  GLuint vbo, program, framebuffer, colorbuffer, depthbuffer, stencilbuffer;
  GLint loc_position, loc_color;

  EGL_SETUP();

  // Set Up Frame Context
  glGenFramebuffers(1, &framebuffer);
  glGenTextures(1, &colorbuffer);
  glGenRenderbuffers(1, &depthbuffer);
  glGenRenderbuffers(1, &stencilbuffer);

  glBindTexture(GL_TEXTURE_2D, colorbuffer);
  glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, WIDTH, HEIGHT);
  
  glBindRenderbuffer(GL_RENDERBUFFER, depthbuffer);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, WIDTH, HEIGHT);
  
  glBindRenderbuffer(GL_RENDERBUFFER, stencilbuffer);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, WIDTH, HEIGHT);
  
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorbuffer, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthbuffer);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, stencilbuffer);

  glViewport(0, 0, WIDTH, HEIGHT); 

  // Set Up Program
  program = glCreateProgram();
  LINK_PROGRAM(program, "kernel");
  glUseProgram(program);

  // Get Vertex Attributes
  loc_position  = glGetAttribLocation(program, "position");
  loc_color     = glGetAttribLocation(program, "in_color");
  
  glVertexAttribPointer(loc_position, 3, GL_FLOAT, GL_FALSE, 0, &triangle_position);
  glEnableVertexAttribArray(loc_position); 
  glVertexAttrib4f(loc_color, 1, 1, 1, 1);

  glEnable(GL_STENCIL_TEST);
  glStencilFunc(GL_NEVER, 1, 0xFF);
  glStencilOp(GL_REPLACE, GL_KEEP, GL_KEEP);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  glDrawArrays(GL_TRIANGLES, 0, 3);

  glVertexAttribPointer(loc_position, 3, GL_FLOAT, GL_FALSE, 0, &quad_position);
  glVertexAttrib4f(loc_color, 1, 0, 0, 1);
  glStencilFunc(GL_GREATER, 1, 0xFF);
  glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  
  
  glVertexAttrib4f(loc_color, 0, 1, 0, 1);
  glStencilFunc(GL_LESS, 0, 0xFF);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  
  // glFinish();
  
  #ifdef C_OPENGL_HOST
  glReadPixels(0,0,WIDTH, HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, result);
  #else
  glReadnPixels(0,0,WIDTH, HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, WIDTH*HEIGHT*4, result);
  #endif

  print_ppm("image.ppm", WIDTH, HEIGHT, (uint8_t*) result);

  EGL_DESTROY();

  return 0; 
}