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
   0.0, 1.0, 0.0,
  -1.0,-1.0, 0.0,
   1.0,-1.0, 0.0,
   0.0, 1.0, 0.0,
  -1.0,-1.0, 0.0,
   1.0,-1.0, 0.0,
};

static float position_strip[] = {
   0.0, 1.0, 1.0,
  -1.0,-1.0, 1.0,
   1.0,-1.0, 1.0,
  -1.0, 1.0, 1.0,
};

static float position_fan[] = {
   0.0, 1.0, 0.5,
  -1.0,-1.0, 0.5,
   1.0,-1.0, 0.5,
  -1.0, 1.0, 0.5,
};

static float color[] = {
   1.0, 0.0, 0.0,
   0.0, 1.0, 0.0,
   0.0, 0.0, 1.0,
   1.0, 0.0, 0.0,
   0.0, 1.0, 0.0,
   0.0, 0.0, 1.0
};

int main() {
  // Set up vertex buffer object
  uint8_t result[WIDTH][HEIGHT][4]; // RGBA8 32 bits x fragment
  GLuint vbo, program, framebuffer, texture, depthbuffer, stencilbuffer;
  GLint loc_position, loc_color;

  #ifdef HOSTDRIVER
  EGL_SETUP();
  #endif

  // Set Up Frame Context
  glGenFramebuffers(1, &framebuffer);
  glGenTextures(1, &texture);
  glGenRenderbuffers(1, &depthbuffer);
  glGenRenderbuffers(1, &stencilbuffer);

  glBindTexture(GL_TEXTURE_2D, texture);
  #ifdef HOSTDRIVER
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, WIDTH, HEIGHT, 0,GL_RGB, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); 
  glBindTexture(GL_TEXTURE_2D, 0);
  printf("Tex error: %x\n", glGetError());
  #else
  glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, WIDTH, HEIGHT);
  #endif

  glBindRenderbuffer(GL_RENDERBUFFER, depthbuffer);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, WIDTH, HEIGHT);

  glBindRenderbuffer(GL_RENDERBUFFER, stencilbuffer);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, WIDTH, HEIGHT);

  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthbuffer);
  //glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, stencilbuffer);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) printf("Error framebuffer: %x\n", glCheckFramebufferStatus(GL_FRAMEBUFFER) );
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
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  
  glEnable(GL_DEPTH_TEST);
  // glEnable(GL_STENCIL_TEST);
  glEnable(GL_SCISSOR_TEST);

  glScissor(0, 0, WIDTH/2, HEIGHT/2);
  glClearColor(1.f,1.f,1.f,1.f);
  glClear(GL_COLOR_BUFFER_BIT);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  // Draw
  glScissor(WIDTH/2, 0, WIDTH/2, HEIGHT/2);
  glClearColor(1.f,1.f,0.f,1.f);
  glClear(GL_COLOR_BUFFER_BIT);
  glDrawArrays(GL_TRIANGLE_FAN, 0, 3);

  glScissor(0, HEIGHT/2, WIDTH/2, HEIGHT/2);
  glClearColor(0.f,1.f,1.f,1.f);
  glClear(GL_COLOR_BUFFER_BIT);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 3);

  glFinish();
  
  #ifdef HOSTDRIVER
  glReadPixels(0,0,WIDTH, HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, result);
  #else
  glReadnPixels(0,0,WIDTH, HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, WIDTH*HEIGHT*4, result);
  #endif
  print_ppm("image.ppm", WIDTH, HEIGHT, (uint8_t*) result);

  return 0; 
}