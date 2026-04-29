#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

#include <unistd.h> 
#include <string.h>

#include "../common.h"

#define WIDTH 1500
#define HEIGHT 1000

static float position[] = {
  -.5, .5, 0.0,
  -.5,-.5, 0.0,
   .5,-.5, 0.0,
  -.5, .5, 0.0,
   .5, .5, 0.0,
   .5,-.5, 0.0,
};
static float texCoord[] = {
   0.0, -4,
   0.0, 0.0,
   4, 0.0,
   0.0, 4,
   4, -4,
   4, 0.0,
};

int main() {
  // Set up vertex buffer object
  uint8_t result[WIDTH][HEIGHT][4]; // RGBA8 32 bits x fragment
  GLuint vbo, program, framebuffer, colorbuffer;
  GLint loc_position, loc_texCoord, loc_sampler;

  EGL_SETUP();

  // Set Up Frame Context
  glGenFramebuffers(1, &framebuffer);
  glGenTextures(1, &colorbuffer);

  glBindTexture(GL_TEXTURE_2D, colorbuffer);
  glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, WIDTH, HEIGHT);
  
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorbuffer, 0);

  glViewport(0, 0, WIDTH, HEIGHT);

  // Set Up Program
  program = glCreateProgram();
  LINK_PROGRAM(program, "kernel");
  glUseProgram(program);

  // Set Up Vertex Attributes
  loc_position  = glGetAttribLocation(program, "position");
  loc_texCoord  = glGetAttribLocation(program, "in_texCoord");
  loc_sampler   = glGetUniformLocation(program, "ourTexture");

  glVertexAttribPointer(loc_position, 3, GL_FLOAT, GL_FALSE, 0, &position);
  glEnableVertexAttribArray(loc_position); 

  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER,sizeof(texCoord), &texCoord, GL_STATIC_DRAW);
  glVertexAttribPointer(loc_texCoord, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
  glEnableVertexAttribArray(loc_texCoord); 
  // glEnable(GL_BLEND);

  glBlendEquation(GL_FUNC_ADD);
  glBlendFunc(GL_SRC_ALPHA, GL_DST_ALPHA);

  // Set up texture
  unsigned int texture;
  // glPixelStorei(GL_UNPACK_ALIGNMENT, 1); 
  glGenTextures(1, &texture);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture);
  glUniform1i(loc_sampler, 0); // texture unit 0
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // has to be enabled or mipmap has to be generated
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  ppm_image_t* image = read_ppm("../assets/dog.ppm");
  uint8_t* data = (uint8_t*) malloc(image->x*image->y*sizeof(uint8_t[4]));
  for(uint32_t i=0; i<image->x*image->y; ++i) {
    data[i*4+0] = image->data[i].red;
    data[i*4+1] = image->data[i].green;
    data[i*4+2] = image->data[i].blue;
    data[i*4+3] = 0x0Fu;
  }

  #ifdef C_OPENGL_HOST
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image->x,image->y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
  // glGenerateMipmap(GL_TEXTURE_2D);
  #else
  glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, image->x, image->y);
  glTexSubImage2D(GL_TEXTURE_2D,0,0,0,image->x,image->y, GL_RGBA, GL_UNSIGNED_BYTE, data);
  #endif
  
  glClear(GL_COLOR_BUFFER_BIT);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  
  #ifdef C_OPENGL_HOST
  glReadPixels(0,0,WIDTH, HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, result);
  #else
  glReadnPixels(0,0,WIDTH, HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, WIDTH*HEIGHT*4, result);
  #endif
  
  print_ppm("image.ppm", WIDTH, HEIGHT, (uint8_t*) result);

  EGL_DESTROY();

  return 0; 
}