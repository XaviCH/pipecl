#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <stdlib.h>     /* srand, rand */
#include <chrono>


#include <unistd.h> 
#include <string.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../common.h"

#define WIDTH 1920
#define HEIGHT 1080

static float position_fan_array[] {
   0, 0, 0,
  -1, 0, 0,
   0, 1, 0,
   1, 0, 0,
   0,-1, 0,
  -1,-0, 0,
};

static float color_fan_array[] {
  1, 1, 1,
  1, 0, 0,
  0, 1, 0,
  0, 0, 1,
  0, 0, 0,
  1, 0, 0,
};

static int fan_index[] {
  0, 1, 2, 3, 4, 1
};

/*
static float position[] = {
  // FRONT
  -0.5, 0.5, -0.5,
  -0.5, -0.5, -0.5,
  0.5, -0.5, -0.5,
  -0.5, 0.5, -0.5,
  0.5, 0.5, -0.5,
  0.5, -0.5, -0.5,
  // BACK
  -0.5, 0.5, 0.5,
  -0.5, -0.5, 0.5,
  0.5, -0.5, 0.5,
  -0.5, 0.5, 0.5,
  0.5, 0.5, 0.5,
  0.5, -0.5, 0.5,
  // TOP
  -0.5, 0.5, 0.5,
  -0.5, 0.5, -0.5,
  0.5, 0.5, -0.5,
  -0.5, 0.5, 0.5,
  0.5, 0.5, 0.5,
  0.5, 0.5, -0.5,
  // BOTTOM
  -0.5, -0.5, 0.5,
  -0.5, -0.5, -0.5,
  0.5, -0.5, -0.5,
  -0.5, -0.5, 0.5,
  0.5, -0.5, 0.5,
  0.5, -0.5, -0.5,
  // LEFT
  -0.5, 0.5, -0.5,
  -0.5, -0.5, -0.5,
  -0.5, -0.5, 0.5,
  -0.5, 0.5, -0.5,
  -0.5, 0.5, 0.5,
  -0.5, -0.5, 0.5,
  // RIGHT
  0.5, 0.5, -0.5,
  0.5, -0.5, -0.5,
  0.5, -0.5, 0.5,
  0.5, 0.5, -0.5,
  0.5, 0.5, 0.5,
  0.5, -0.5, 0.5,
};

static float color[] = {
  // FRONT
  1.0, 0.0, 0.0,
  0.0, 1.0, 0.0,
  0.0, 0.0, 1.0,
  1.0, 0.0, 0.0,
  0.0, 1.0, 0.0,
  0.0, 0.0, 1.0,
  // BACK
  1.0, 0.0, 0.0,
  0.0, 1.0, 0.0,
  0.0, 0.0, 1.0,
  1.0, 0.0, 0.0,
  0.0, 1.0, 0.0,
  0.0, 0.0, 1.0,
  // TOP
  1.0, 0.0, 0.0,
  0.0, 1.0, 0.0,
  0.0, 0.0, 1.0,
  1.0, 0.0, 0.0,
  0.0, 1.0, 0.0,
  0.0, 0.0, 1.0,
  // BOTTOM
  1.0, 0.0, 0.0,
  0.0, 1.0, 0.0,
  0.0, 0.0, 1.0,
  1.0, 0.0, 0.0,
  0.0, 1.0, 0.0,
  0.0, 0.0, 1.0,
  // LEFT
  1.0, 0.0, 0.0,
  0.0, 1.0, 0.0,
  0.0, 0.0, 1.0,
  1.0, 0.0, 0.0,
  0.0, 1.0, 0.0,
  0.0, 0.0, 1.0,
  // RIGHT
  1.0, 0.0, 0.0,
  0.0, 1.0, 0.0,
  0.0, 0.0, 1.0,
  1.0, 0.0, 0.0,
  0.0, 1.0, 0.0,
  0.0, 0.0, 1.0,
};
*/
GLushort INDEX_BUFFER_TRIANGLES[] = {
    0, 1, 3,
    3, 1, 2,
    1, 5, 2,
    2, 5, 6,
    5, 4, 6, 6, 4, 7,
    4, 0, 7, 7, 0, 3,
    3, 2, 7, 7, 2, 6,
    4, 5, 0, 0, 5, 1,
};

GLushort INDEX_BUFFER_TRIANGLE_FAN[] = {
    0, 1, 3, 3, 1, 2,
    1, 5, 2, 2, 5, 6,
    5, 4, 6, 6, 4, 7,
    4, 0, 7, 7, 0, 3,
    3, 2, 7, 7, 2, 6,
    4, 5, 0, 0, 5, 1,
};

GLushort INDEX_BUFFER_TRIANGLE_STRIP[] = {
    0, 1, 3, 3, 1, 2,
    1, 5, 2, 2, 5, 6,
    5, 4, 6, 6, 4, 7,
    4, 0, 7, 7, 0, 3,
    3, 2, 7, 7, 2, 6,
    4, 5, 0, 0, 5, 1,
};

float position_array[] = {
    -1, -1, -1,
    1, -1, -1,
    1, 1, -1, 
    -1, 1, -1,

    -1, -1, 1, 
    1, -1, 1, 
    1, 1, 1, 
    -1, 1, 1,
};

float color_array[] {
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
  // uint8_t result[WIDTH][HEIGHT][4]; // RGBA8 32 bits x fragment
  GLuint program, framebuffer, colorbuffer, depthbuffer, stencilbuffer;
  GLint loc_position, loc_color, loc_sampler, loc_texCoord;
  
  // Set Up Frame Context
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
  // 

  glViewport(0, 0, WIDTH, HEIGHT); 

  // Set Up Program
  program = glCreateProgram();
  LINK_PROGRAM(program, "kernel");
  glUseProgram(program);

  // Set Up Vertex Attributes
  loc_position  = glGetAttribLocation(program, "position");
  loc_color     = glGetAttribLocation(program, "color");
  
  // Client memory
  // glVertexAttribPointer(loc_position, 3, GL_FLOAT, GL_FALSE, 0, &position);
  // glEnableVertexAttribArray(loc_position); 
  // // Server memory
  // GLuint vbo;
  // glGenBuffers(1, &vbo);
  // glBindBuffer(GL_ARRAY_BUFFER, vbo);
  // glBufferData(GL_ARRAY_BUFFER,sizeof(color), &color, GL_STATIC_DRAW);
  // glVertexAttribPointer(loc_color, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
  // glEnableVertexAttribArray(loc_color); 

  // Uniforms
  GLint loc_perspective, loc_view, loc_model;
  
  loc_perspective = glGetUniformLocation(program, "perspective");
  loc_view = glGetUniformLocation(program, "view");
  loc_model = glGetUniformLocation(program, "model");

  printf("pers: %i , view: %i, model: %i\n", loc_perspective, loc_view, loc_model);

  glm::mat4 perspective(1), view(1), model(1);

  glUniformMatrix4fv(loc_perspective, 1, GL_FALSE, &perspective[0][0]);
  glUniformMatrix4fv(loc_view, 1, GL_FALSE, &view[0][0]);
  glUniformMatrix4fv(loc_model, 1, GL_FALSE, &model[0][0]);
  
  // Clear scene

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  // Draw Fan Stencil
  // glEnable(GL_STENCIL_TEST);
  // glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
  // glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
  // glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  
  // glVertexAttribPointer(loc_position, 3, GL_FLOAT, GL_FALSE, 0, &position_fan_array);
  // glEnableVertexAttribArray(loc_position);
  // glVertexAttribPointer(loc_color, 3, GL_FLOAT, GL_FALSE, 0, &color_fan_array);
  // glEnableVertexAttribArray(loc_color); 

  // model = glm::translate(glm::mat4(1), glm::vec3{-.5,0,0});
  // glUniformMatrix4fv(loc_model, 1, GL_FALSE, &model[0][0]);

  // glDrawArrays(GL_TRIANGLE_FAN, 0, sizeof(position_fan_array)/sizeof(position_fan_array[0])/3);
  // model = glm::translate(glm::mat4(1), glm::vec3{.5,0,0});
  // glUniformMatrix4fv(loc_model, 1, GL_FALSE, &model[0][0]);
  // glDrawRangeElements(GL_TRIANGLE_FAN, 0, sizeof(position_fan_array)/sizeof(position_fan_array[0])/3, sizeof(fan_index)/sizeof(fan_index[0]), GL_UNSIGNED_SHORT, fan_index);

  // Draw Cubes on Stencil
  // glEnale(GL_STENCIL_TEST);
  // glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
  // glStencilFunc(GL_EQUAL, 1, 0xFF);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);

  glVertexAttribPointer(loc_position, 3, GL_FLOAT, GL_FALSE, 0, &position_array);
  glEnableVertexAttribArray(loc_position);
  glVertexAttribPointer(loc_color, 3, GL_FLOAT, GL_FALSE, 0, &color_array);
  glEnableVertexAttribArray(loc_color);

  perspective = glm::perspective((float)M_PI / 4, (float)WIDTH/HEIGHT, 0.1f, 100.f);
  view = glm::lookAt(glm::vec3{0, 0, -2}, glm::vec3{0,0,0},glm::vec3{0,1,0});

  glUniformMatrix4fv(loc_perspective, 1, GL_FALSE, &perspective[0][0]);
  glUniformMatrix4fv(loc_view, 1, GL_FALSE, &view[0][0]);

  size_t cubes_number = 1000;

  auto begin = std::chrono::high_resolution_clock::now();
  for (int cube_id = 0; cube_id < cubes_number; ++cube_id) {
    const int scale = 50;
    float x = ((double) rand() / RAND_MAX) * scale - scale/2;
    float y = ((double) rand() / RAND_MAX) * scale - scale/2;
    float z = ((double) rand() / RAND_MAX) * scale;
    float size = ((double) rand() / RAND_MAX) + 0.25;
    float angle = ((double) rand() / RAND_MAX) * 2 * M_PI;

    model = glm::mat4(1);
    model = glm::translate(model, glm::vec3(x, y, z));
    model = glm::rotate(model, angle, glm::vec3(1));
    model = glm::scale(model, glm::vec3(1));
    glUniformMatrix4fv(loc_model, 1, GL_FALSE, &model[0][0]);

    size_t count = sizeof(INDEX_BUFFER_TRIANGLES)/sizeof(INDEX_BUFFER_TRIANGLES[0]);
    glDrawRangeElements(GL_TRIANGLES, 0, sizeof(position_array)/sizeof(position_array[0])/3, count, GL_UNSIGNED_SHORT, &INDEX_BUFFER_TRIANGLES);
  }
  glFinish();
  auto end = std::chrono::high_resolution_clock::now();
  double total_time_microseconds = (double)std::chrono::duration_cast<std::chrono::microseconds>(end-begin).count();

  printf("INFO: cubes_number=%d.\n", cubes_number);
  printf("PERF: time=%.3fms.\n", total_time_microseconds/(1000));
  
  uint8_t* result = (uint8_t*) malloc(sizeof(uint8_t[WIDTH][HEIGHT][4]));

  #ifdef C_OPENGL_HOST
  glReadPixels(0,0,WIDTH, HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, result);
  #else
  glReadnPixels(0,0,WIDTH, HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, WIDTH*HEIGHT*4, result);
  #endif

  print_ppm("image.ppm", WIDTH, HEIGHT, (uint8_t*) result);

  EGL_DESTROY();

  return 0; 
}