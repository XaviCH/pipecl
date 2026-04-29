#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

#include <unistd.h> 
#include <string.h>

#include "../common.h"

#define WIDTH 150
#define HEIGHT 100

static float position[] = {
  -.5, .5, 0.0,
  -.5,-.5, 0.0,
   .5,-.5, 0.0,
  -.5, .5, 0.0,
   .5, .5, 0.0,
   .5,-.5, 0.0,
};
static float color[] = {
   1.0, 0.0, 0.0,
   0.0, 1.0, 0.0,
   0.0, 0.0, 1.0,
   1.0, 0.0, 0.0,
   0.0, 1.0, 0.0,
   0.0, 0.0, 1.0,
};

int main() {
  // Set up vertex buffer object
  uint8_t result[WIDTH][HEIGHT][4]; // RGBA8 32 bits x fragment
  GLuint vbo, program, framebuffer, colorbuffer;
  GLint loc_position, loc_color;

  #ifdef HOSTDRIVER
  EGL_SETUP();
  #endif

  // Set Up Frame Context
  glGenFramebuffers(1, &framebuffer);
  glGenRenderbuffers(1, &colorbuffer);

  glBindRenderbuffer(GL_RENDERBUFFER, colorbuffer);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA4, WIDTH, HEIGHT);

  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorbuffer);

  glViewport(0, 0, WIDTH, HEIGHT); 

  // Set Up Program
  program = glCreateProgram();
  file_t file;
  #ifndef HOSTDRIVER
  read_file("kernel.pocl", &file);
  glProgramBinary(program, 0, file.data, file.size);
  #else
  GLuint vs, fs;
  GLchar *bin;
  GLint size;
  GLint success;

  vs = glCreateShader(GL_VERTEX_SHADER);
  fs = glCreateShader(GL_FRAGMENT_SHADER);

  read_file("kernel.vs", &file);
  bin = (char*)file.data;
  size = file.size;

  glShaderSource(vs, 1, &bin, &size);
  glCompileShader(vs);
  free(bin);

  glGetShaderiv(vs, GL_COMPILE_STATUS, &success);  
  if (success == GL_FALSE) {
    GLint logSize = 0;
    glGetShaderiv(vs, GL_INFO_LOG_LENGTH, &logSize);

    GLchar* infoLog = (GLchar*) malloc(logSize);
    glGetShaderInfoLog(vs, logSize, &logSize, infoLog);
    printf("Fail compile vs: %s\n", infoLog);
    exit(-1);
  }
  
  read_file("kernel.fs", &file);
  bin = (char*)file.data;
  size = file.size;

  glShaderSource(fs, 1, &bin, &size);
  glCompileShader(fs);

  glGetShaderiv(fs, GL_COMPILE_STATUS, &success);  
  if (success == GL_FALSE) {
    GLint logSize = 0;
    glGetShaderiv(fs, GL_INFO_LOG_LENGTH, &logSize);

    GLchar* infoLog = (GLchar*) malloc(logSize);
    glGetShaderInfoLog(fs, logSize, &logSize, infoLog);
    printf("Fail compile fs: %s\n", infoLog);
    exit(-1);
  }

  free(file.data);

  glAttachShader(program, vs);
  glAttachShader(program, fs);

  glLinkProgram(program);

  glDetachShader(program, vs);
  glDetachShader(program, fs);
  glDeleteShader(vs);
  glDeleteShader(fs);
  #endif
  glUseProgram(program);

  // Set Up Vertex Attributes
  loc_position  = glGetAttribLocation(program, "position");
  loc_color     = glGetAttribLocation(program, "in_color");
  
  glVertexAttribPointer(loc_position, 3, GL_FLOAT, GL_FALSE, 0, &position);
  glEnableVertexAttribArray(loc_position); 

  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER,sizeof(color), &color, GL_STATIC_DRAW);
  glVertexAttribPointer(loc_color, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
  glEnableVertexAttribArray(loc_color); 
  // glEnable(GL_DITHER);
  // Draw
  glClear(GL_COLOR_BUFFER_BIT);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glFinish();
  
  #ifdef HOSTDRIVER
  glReadPixels(0,0,WIDTH, HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, result);
  #else
  glReadnPixels(0,0,WIDTH, HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, WIDTH*HEIGHT*4, result);
  #endif
  print_ppm("image.ppm", WIDTH, HEIGHT, (uint8_t*) result);

  return 0; 
}