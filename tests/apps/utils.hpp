#ifndef TESTS_GLSC2_UTILS_HPP
#define TESTS_GLSC2_UTILS_HPP

#include <GLSC2/glsc2.h>
#include <EGL/egl.h>

#include "parameters.h"

namespace tests {

static void egl_setup(EGLDisplay* display, EGLSurface* surface)
{
  *display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  
  EGLint major, minor;
  eglInitialize(*display, &major, &minor);

  eglBindAPI(EGL_OPENGL_API);
  EGLint configAttribs[] = {
    EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_ALPHA_SIZE, 8,
    EGL_DEPTH_SIZE, 16,
    EGL_STENCIL_SIZE, 8,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
    EGL_NONE
  };

  EGLConfig config;
  EGLint numConfigs;
  eglChooseConfig(*display, configAttribs, &config, 1, &numConfigs);

  EGLint contextAttribs[] = {
      EGL_CONTEXT_MAJOR_VERSION, 3,
      EGL_CONTEXT_MINOR_VERSION, 0,
      EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
      EGL_NONE
  };
  EGLContext eglContext = eglCreateContext(*display, config, EGL_NO_CONTEXT, contextAttribs);

  EGLint surfaceAttribs[] = {
      EGL_WIDTH, static_cast<int>(test_get_width()),
      EGL_HEIGHT, static_cast<int>(test_get_height()),
      EGL_NONE 
  };
  
  *surface = eglCreatePbufferSurface(*display, config, surfaceAttribs);

  eglMakeCurrent(*display, *surface, *surface, eglContext); 
}

static void offline_setup(GLuint *framebuffer, GLuint *colorbuffer, GLuint *depthbuffer, GLuint *stencilbuffer)
{
  // Set Up Frame Context
  glGenFramebuffers(1, framebuffer);
  glGenTextures(1, colorbuffer);
  glGenRenderbuffers(1, depthbuffer);
  glGenRenderbuffers(1, stencilbuffer);

  glBindTexture(GL_TEXTURE_2D, *colorbuffer);
  glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, test_get_width(), test_get_height());

  glBindRenderbuffer(GL_RENDERBUFFER, *depthbuffer);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, test_get_width(), test_get_height());

  glBindRenderbuffer(GL_RENDERBUFFER, *stencilbuffer);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, test_get_width(), test_get_height());

  glBindFramebuffer(GL_FRAMEBUFFER, *framebuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *colorbuffer, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, *depthbuffer);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, *stencilbuffer);
}

static void setup_egl_and_framebuffers(
  EGLDisplay* display, EGLSurface* surface,
  GLuint *framebuffer, GLuint *colorbuffer, GLuint *depthbuffer, GLuint *stencilbuffer
)
{
  egl_setup(display, surface);
  
  if (test_is_offline_rendering()) 
  {
    offline_setup(framebuffer, colorbuffer, depthbuffer, stencilbuffer);
  }
}

static void swap_buffers_if_needed(EGLDisplay display, EGLSurface surface) 
{
  if (test_is_offline_rendering()) 
  {
    // do nothing

  } else {
    eglSwapBuffers(display, surface);
  }
}

#include "common.h"

static void save_framebuffer_as_ppm(const char* filename)
{
  uint8_t* result = (uint8_t*) malloc(sizeof(uint8_t[test_get_width()][test_get_height()][4]));

  #ifdef C_OPENGL_HOST
  glReadPixels(0,0,test_get_width(), test_get_height(), GL_RGBA, GL_UNSIGNED_BYTE, result);
  #else
  glReadnPixels(0,0,test_get_width(), test_get_height(), GL_RGBA, GL_UNSIGNED_BYTE, test_get_width()*test_get_height()*4, result);
  #endif

  print_ppm(filename, test_get_width(), test_get_height(), (uint8_t*) result);
  free(result);
}

};

#endif // TESTS_GLSC2_UTILS_HPP