#include <stdio.h>
#include <stdlib.h>
#include <EGL/egl.h>

// #ifdef HOSTDRIVER
#define EGL_EGLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#include <EGL/eglext.h>
#ifdef C_OPENGL_HOST
#include <GLES2/gl2.h>
#else
#include <GLSC2/glsc2.h>
#endif

#ifdef C_OPENCL_HOST
#define KERNEL_FILE_EXT ".cl.o"
#endif

#ifndef GL_RGBA8
#define GL_RGBA8 GL_RGBA
#endif

#ifdef C_OPENGL_HOST
#define EGL_SETUP() \
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY); \
    EGLint major; \
    EGLint minor; \
    eglInitialize(display, &major, &minor); \
    eglBindAPI(EGL_OPENGL_API); \
    EGLint configAttribs[] = { \
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, \
        EGL_RED_SIZE, 8, \
        EGL_GREEN_SIZE, 8, \
        EGL_BLUE_SIZE, 8, \
        EGL_ALPHA_SIZE, 8, \
        EGL_DEPTH_SIZE, 16, \
        EGL_STENCIL_SIZE, 8, \
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, \
        EGL_NONE \
    }; \
    EGLConfig config; \
    EGLint numConfigs; \
    eglChooseConfig(display, configAttribs, &config, 1, &numConfigs); \
    EGLint contextAttribs[] = { \
        EGL_CONTEXT_MAJOR_VERSION, 3, \
        EGL_CONTEXT_MINOR_VERSION, 0, \
        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, \
        EGL_NONE \
    }; \
    EGLContext eglContext = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs); \
    EGLint surfaceAttribs[] = { \
        EGL_WIDTH, static_cast<int>(WIDTH), \
        EGL_HEIGHT, static_cast<int>(HEIGHT), \
        EGL_NONE \
    }; \
    EGLSurface surface = eglCreatePbufferSurface(display, config, surfaceAttribs); \
    eglMakeCurrent(display, surface, surface, eglContext); 
#else
#define EGL_SETUP()
#endif

#ifdef C_OPENGL_HOST
#define EGL_DESTROY() \
    eglDestroyContext(display, eglContext); \
    eglDestroySurface(display, surface); \
    eglTerminate(display);
#else
#define EGL_DESTROY()
#endif

#ifdef C_OPENGL_HOST
#define LINK_PROGRAM(_PROGRAM, _SHADER_NAME) \
{ \
  file_t file; \
  GLuint vs, fs; \
  GLint success; \
  vs = glCreateShader(GL_VERTEX_SHADER); \
  fs = glCreateShader(GL_FRAGMENT_SHADER); \
  read_file(_SHADER_NAME ".vs", &file); \
  glShaderSource(vs, 1, (const GLchar* const*) &file.data, (const int*) &file.size); \
  glCompileShader(vs); \
  free(file.data); \
  glGetShaderiv(vs, GL_COMPILE_STATUS, &success);   \
  if (success == GL_FALSE) { \
    GLint logSize = 0; \
    glGetShaderiv(vs, GL_INFO_LOG_LENGTH, &logSize); \
    GLchar* infoLog = (GLchar*) malloc(logSize); \
    glGetShaderInfoLog(vs, logSize, &logSize, infoLog); \
    printf("Fail compile vs: %s\n", infoLog); \
    exit(-1); \
  } \
  read_file(_SHADER_NAME ".fs", &file); \
  glShaderSource(fs, 1,(const GLchar* const*) &file.data, (const int*) &file.size); \
  glCompileShader(fs); \
  glGetShaderiv(fs, GL_COMPILE_STATUS, &success);   \
  if (success == GL_FALSE) { \
    GLint logSize = 0; \
    glGetShaderiv(fs, GL_INFO_LOG_LENGTH, &logSize); \
    GLchar* infoLog = (GLchar*) malloc(logSize); \
    glGetShaderInfoLog(fs, logSize, &logSize, infoLog); \
    printf("Fail compile fs: %s\n", infoLog); \
    exit(-1); \
  } \
  free(file.data); \
  glAttachShader(_PROGRAM, vs); \
  glAttachShader(_PROGRAM, fs); \
  glLinkProgram(_PROGRAM); \
  glDetachShader(_PROGRAM, vs); \
  glDetachShader(_PROGRAM, fs); \
  glDeleteShader(vs); \
  glDeleteShader(fs); \
} 
#else
#define LINK_PROGRAM(_PROGRAM, _SHADER_NAME) \
{ \
  file_t file; \
  read_file(_SHADER_NAME KERNEL_FILE_EXT, &file); \
  glProgramBinary(_PROGRAM, 0, file.data, file.size); \
  free(file.data); \
}
#endif

// IMAGES
typedef struct {
     unsigned char red,green,blue;
} ppm_pixel_t;

typedef struct {
  int x, y;
  ppm_pixel_t *data;
} ppm_image_t;

#define RGB_COMPONENT_COLOR 255

static ppm_image_t *read_ppm(const char *filename)
{
  char buff[16];
  ppm_image_t *img;
  FILE *fp;
  int c, rgb_comp_color;
  //open PPM file for reading
  fp = fopen(filename, "rb");
  if (!fp) {
    fprintf(stderr, "Unable to open file '%s'\n", filename);
    exit(1);
  }

  //read image format
  if (!fgets(buff, sizeof(buff), fp)) {
    perror(filename);
    exit(1);
  }

  //check the image format
  if (buff[0] != 'P' || buff[1] != '6') {
    fprintf(stderr, "Invalid image format (must be 'P6')\n");
    exit(1);
  }

  //alloc memory form image
  img = (ppm_image_t *)malloc(sizeof(ppm_image_t));
  if (!img) {
    fprintf(stderr, "Unable to allocate memory\n");
    exit(1);
  }

  //check for comments
  c = getc(fp);
  while (c == '#') {
  while (getc(fp) != '\n') ;
        c = getc(fp);
  }

  ungetc(c, fp);
  //read image size information
  if (fscanf(fp, "%d %d", &img->x, &img->y) != 2) {
        fprintf(stderr, "Invalid image size (error loading '%s')\n", filename);
        exit(1);
  }

  //read rgb component
  if (fscanf(fp, "%d", &rgb_comp_color) != 1) {
        fprintf(stderr, "Invalid rgb component (error loading '%s')\n", filename);
        exit(1);
  }

  //check rgb component depth
  if (rgb_comp_color!= RGB_COMPONENT_COLOR) {
        fprintf(stderr, "'%s' does not have 8-bits components\n", filename);
        exit(1);
  }

  while (fgetc(fp) != '\n') ;
  //memory allocation for pixel data
  img->data = (ppm_pixel_t*)malloc(img->x * img->y * sizeof(ppm_pixel_t));

  if (!img) {
        fprintf(stderr, "Unable to allocate memory\n");
        exit(1);
  }

  //read pixel data from file
  if (fread(img->data, 3 * img->x, img->y, fp) != img->y) {
        fprintf(stderr, "Error loading image '%s'\n", filename);
        exit(1);
  }

  fclose(fp);
  return img;
}

static void print_ppm(const char* filename, size_t width, size_t height, const uint8_t *data) {
  FILE *f = fopen(filename, "wb");
  fprintf(f, "P6\n%zu %zu 255\n", width, height);
  for (size_t y=0; y<height; y++) {
      for (size_t x=0; x<width; x++) {
          fputc(data[0], f); 
          fputc(data[1], f); // 0 .. 255
          fputc(data[2], f);  // 0 .. 255
          data += 4;
      }
  }
  fclose(f);
}


// FILES

typedef struct {
    void* data;
    size_t size;
} file_t;

static int read_file(const char* filename, file_t* file) {
  if (NULL == filename || NULL == file)
    return -1;

  FILE* fp = fopen(filename, "r");
  if (NULL == fp) {
    fprintf(stderr, "Failed to load the file %s.\n", filename);
    return -1;
  }
  fseek(fp , 0 , SEEK_END);
  long fsize = ftell(fp);
  rewind(fp);
  
  file->data = malloc(fsize);
  file->size = fread(file->data, 1, fsize, fp);
  
  fclose(fp);
  
  return 0;
}