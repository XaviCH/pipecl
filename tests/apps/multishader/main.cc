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

#define WIDTH 2048
#define HEIGHT 2048

GLushort INDEX_BUFFER[] = {
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

float color[] = {
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
  GLuint vbo[2], program[2], framebuffer, colorbuffer, depthbuffer;

  EGL_SETUP();

  // Set Up Frame Context
  glGenFramebuffers(1, &framebuffer);
  glGenTextures(1, &colorbuffer);
  glGenRenderbuffers(1, &depthbuffer);

  glBindTexture(GL_TEXTURE_2D, colorbuffer);
  glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, WIDTH, HEIGHT);

  glBindRenderbuffer(GL_RENDERBUFFER, depthbuffer);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, WIDTH, HEIGHT);

  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorbuffer, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthbuffer);

  glViewport(0, 0, WIDTH, HEIGHT);

  // Set Up Program
  program[0] = glCreateProgram();
  program[1] = glCreateProgram();
  LINK_PROGRAM(program[0], "kernel_tex");
  LINK_PROGRAM(program[1], "kernel_blending");
  
  // Get Vertex Attributes Locations
  GLint loc_position[2], loc_color, loc_texCoord;

  loc_position[0]  = glGetAttribLocation(program[0], "position");
  loc_texCoord  = glGetAttribLocation(program[0], "texCoord");
  
  loc_position[1]  = glGetAttribLocation(program[1], "position");
  loc_color     = glGetAttribLocation(program[1], "color");
  
  // Get Vertex Attributes Locations
  GLint loc_perspective[2], loc_view[2], loc_model[2], loc_blend, loc_sampler;
  
  loc_perspective[0] = glGetUniformLocation(program[0], "perspective");
  loc_view[0] = glGetUniformLocation(program[0], "view");
  loc_model[0] = glGetUniformLocation(program[0], "model");
  loc_sampler   = glGetUniformLocation(program[0], "ourTexture");
  
  loc_perspective[1] = glGetUniformLocation(program[1], "perspective");
  loc_view[1] = glGetUniformLocation(program[1], "view");
  loc_model[1] = glGetUniformLocation(program[1], "model");
  loc_blend     = glGetUniformLocation(program[1], "blend");
  
  // Server memory
  glGenBuffers(2, vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
  glBufferData(GL_ARRAY_BUFFER,sizeof(color), &color, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
  glBufferData(GL_ARRAY_BUFFER,sizeof(texCoord), &texCoord, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  // Common render config
  glDisable(GL_DITHER);

  // Set up texture
  unsigned int texture;
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1); 
  glGenTextures(1, &texture);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture);

  ppm_image_t* image = read_ppm("../assets/dog.ppm");
  uint8_t* data = (uint8_t*) malloc(image->x*image->y*sizeof(uint8_t[4]));
  for(uint32_t i=0; i<image->x*image->y; ++i) {
    data[i*4+0] = image->data[i].red;
    data[i*4+1] = image->data[i].green;
    data[i*4+2] = image->data[i].blue;
    data[i*4+3] = 0xFFu;
  }

  #ifdef C_OPENGL_HOST
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image->x,image->y, 0, GL_RGB, GL_UNSIGNED_BYTE, image->data);
  #else
  glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, image->x, image->y);
  glTexSubImage2D(GL_TEXTURE_2D,0,0,0,image->x,image->y, GL_RGBA, GL_UNSIGNED_BYTE, data);
  #endif

  glm::mat4 perspective(1), view(1), model0(1), model1(1);
  perspective = glm::perspective((float)M_PI / 4, (float)WIDTH/HEIGHT, 0.1f, 100.f);
  view = glm::lookAt(glm::vec3{5, 5, -5}, glm::vec3{0,0,0},glm::vec3{0,1,0});

  uint32_t sample = 1000;
  auto begin = std::chrono::high_resolution_clock::now();
  for(int i=0; i<sample; ++i)
  {
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   
    glUseProgram(program[0]);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    // Set up vertex attrib
    glVertexAttribPointer(loc_position[0], 3, GL_FLOAT, GL_FALSE, 0, &position);
    glEnableVertexAttribArray(loc_position[0]);

    glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    glVertexAttribPointer(loc_texCoord, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(loc_texCoord);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    model0 = glm::rotate(model0, (float)M_PI/16, glm::vec3{1,1,1});

    glUniformMatrix4fv(loc_perspective[0], 1, GL_FALSE, &perspective[0][0]);
    glUniformMatrix4fv(loc_view[0], 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(loc_model[0], 1, GL_FALSE, &model0[0][0]);
    glUniform1i(loc_sampler, 0);
    // printf("DrawRangeElements\n");
    glDrawRangeElements(GL_TRIANGLES, 0, sizeof(position)/sizeof(position[0])/3, sizeof(INDEX_BUFFER)/sizeof(INDEX_BUFFER[0]), GL_UNSIGNED_SHORT, INDEX_BUFFER);
    
    // Render Second Cube
    glUseProgram(program[1]);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);

    // Set up vertex attrib
    glVertexAttribPointer(loc_position[1], 3, GL_FLOAT, GL_FALSE, 0, &position);
    glEnableVertexAttribArray(loc_position[1]);

    glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
    glVertexAttribPointer(loc_color, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(loc_color); 
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    model1 = glm::rotate(model1, (float)M_PI/32, glm::vec3{1,1,1});

    glUniformMatrix4fv(loc_perspective[1], 1, GL_FALSE, &perspective[0][0]);
    glUniformMatrix4fv(loc_view[1], 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(loc_model[1], 1, GL_FALSE, &model1[0][0]);
    glUniform1f(loc_blend, 0.5f);

    // printf("DrawRangeElements\n");
    glDrawRangeElements(GL_TRIANGLES, 0, sizeof(position)/sizeof(position[0])/3, sizeof(INDEX_BUFFER)/sizeof(INDEX_BUFFER[0]), GL_UNSIGNED_SHORT, INDEX_BUFFER);
  }
  glFinish();
  auto end = std::chrono::high_resolution_clock::now();

  double microseconds_per_second = 1000000.0;
  double total_time_microseconds = (double)std::chrono::duration_cast<std::chrono::microseconds>(end-begin).count();
  double frame_mean_time = total_time_microseconds / (double)sample;
  
  printf("INFO: sampled_size=%d.\n", sample);
  printf("PERF: frame_mean_time=%.3fms. FPS=%d\n", frame_mean_time/(1000), (int)(microseconds_per_second/frame_mean_time));

  uint8_t* result = (uint8_t*) malloc(sizeof(uint8_t[WIDTH][HEIGHT][4])); // RGBA8 32 bits x fragment

  #ifdef C_OPENGL_HOST
    glReadPixels(0,0,WIDTH, HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, result);
  #else
    glReadnPixels(0,0,WIDTH, HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, WIDTH*HEIGHT*4, result);
  #endif
  print_ppm("image.ppm", WIDTH, HEIGHT, (uint8_t*) result);

  EGL_DESTROY();

  return 0; 
}