#include <EGL/egl.h>
#include <GLSC2/glsc2.h>

#include <X11/Xlib.h>
#include <stdlib.h>
#include <stdio.h>

#define NOT_IMPLEMENTED             \
    {                               \
        printf("Funtion %s at %s:%d is not implemented.\n", __func__, __FILE__, __LINE__); \
        exit(1);                    \
    }

typedef struct {
    Display* display;
    Window window;
    GC gc;
    XImage *image;
    void* pixels;
    size_t width, height;

    GLuint current_framebuffer;
    GLuint framebuffer_ids[2];
    GLuint colorbuffers_ids[2];
    GLuint depthbuffers_ids[2];
    GLuint stencilbuffers_ids[2];

    void* orch;
} __egl_context_t;

__egl_context_t egl_context;

__attribute__((constructor))
static void __context_constructor__() 
{
    egl_context.display = XOpenDisplay(NULL);

    if (!egl_context.display) {
        fprintf(stderr, "Cannot open X display\n");
        exit(1);
    }
}

EGLAPI EGLBoolean EGLAPIENTRY eglChooseConfig (EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config) 
{
    if (dpy != egl_context.display) NOT_IMPLEMENTED;

    return EGL_TRUE; 
}

EGLAPI EGLBoolean EGLAPIENTRY eglCopyBuffers (EGLDisplay dpy, EGLSurface surface, EGLNativePixmapType target) { return 0; }
EGLAPI EGLContext EGLAPIENTRY eglCreateContext (EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint *attrib_list) 
{ 
    if (dpy != egl_context.display) NOT_IMPLEMENTED;

    return &egl_context;
}
EGLAPI EGLSurface EGLAPIENTRY eglCreatePbufferSurface (EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list) 
{ 
    if (dpy != egl_context.display) NOT_IMPLEMENTED;

    egl_context.width    = attrib_list[1];
    egl_context.height   = attrib_list[3];

    glGenFramebuffers(2, egl_context.framebuffer_ids);
    glGenTextures(2, egl_context.colorbuffers_ids);
    glGenRenderbuffers(2, egl_context.depthbuffers_ids);
    glGenRenderbuffers(2, egl_context.stencilbuffers_ids);

    for (int buffer=0; buffer < 2; ++buffer)
    {
        glBindTexture(GL_TEXTURE_2D, egl_context.colorbuffers_ids[buffer]);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, egl_context.width, egl_context.height);
        
        glBindRenderbuffer(GL_RENDERBUFFER, egl_context.depthbuffers_ids[buffer]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, egl_context.width, egl_context.height);
        
        glBindRenderbuffer(GL_RENDERBUFFER, egl_context.stencilbuffers_ids[buffer]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, egl_context.width, egl_context.height);
        
        glBindFramebuffer(GL_FRAMEBUFFER, egl_context.framebuffer_ids[buffer]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, egl_context.colorbuffers_ids[buffer], 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, egl_context.depthbuffers_ids[buffer]);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, egl_context.stencilbuffers_ids[buffer]);
    }


    int screen = DefaultScreen(egl_context.display);

    egl_context.window = XCreateSimpleWindow(egl_context.display,
                                  RootWindow(egl_context.display, screen),
                                  0, 0,
                                  egl_context.width, egl_context.height,
                                  1,
                                  BlackPixel(egl_context.display, screen),
                                  WhitePixel(egl_context.display, screen));

    XSelectInput(egl_context.display, egl_context.window, ExposureMask | KeyPressMask);
    XMapWindow(egl_context.display, egl_context.window);

    egl_context.gc = DefaultGC(egl_context.display, screen);

    egl_context.pixels = malloc(sizeof(uint32_t[egl_context.width][egl_context.height]));

    egl_context.image = XCreateImage(egl_context.display,
                         DefaultVisual(egl_context.display, screen),
                         24,
                         ZPixmap,
                         0,
                         (char*)egl_context.pixels,
                         egl_context.width,
                         egl_context.height,
                         32,
                         0);

    return &egl_context.window; 
}
EGLAPI EGLSurface EGLAPIENTRY eglCreatePixmapSurface (EGLDisplay dpy, EGLConfig config, EGLNativePixmapType pixmap, const EGLint *attrib_list) { return 0; }
EGLAPI EGLSurface EGLAPIENTRY eglCreateWindowSurface (EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint *attrib_list) { return 0; }
EGLAPI EGLBoolean EGLAPIENTRY eglDestroyContext (EGLDisplay dpy, EGLContext ctx) { return 0; }
EGLAPI EGLBoolean EGLAPIENTRY eglDestroySurface (EGLDisplay dpy, EGLSurface surface) { return 0; }
EGLAPI EGLBoolean EGLAPIENTRY eglGetConfigAttrib (EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value) { return 0; }
EGLAPI EGLBoolean EGLAPIENTRY eglGetConfigs (EGLDisplay dpy, EGLConfig *configs, EGLint config_size, EGLint *num_config) { return 0; }
EGLAPI EGLDisplay EGLAPIENTRY eglGetCurrentDisplay (void) { return 0; }
EGLAPI EGLSurface EGLAPIENTRY eglGetCurrentSurface (EGLint readdraw) { return 0; }

EGLAPI EGLDisplay EGLAPIENTRY eglGetDisplay (EGLNativeDisplayType display_id) 
{
    if (display_id == EGL_DEFAULT_DISPLAY) return egl_context.display;
    
    NOT_IMPLEMENTED;
}

EGLAPI EGLint EGLAPIENTRY eglGetError (void) { return 0; }
EGLAPI __eglMustCastToProperFunctionPointerType EGLAPIENTRY eglGetProcAddress (const char *procname) { return 0; }

EGLAPI EGLBoolean EGLAPIENTRY eglInitialize (EGLDisplay dpy, EGLint *major, EGLint *minor) 
{ 
    if (dpy != egl_context.display) NOT_IMPLEMENTED;

    return EGL_TRUE; 
}
EGLAPI EGLBoolean EGLAPIENTRY eglMakeCurrent (EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx) 
{
    if (dpy != egl_context.display) NOT_IMPLEMENTED;
    if (draw != &egl_context.window) NOT_IMPLEMENTED;
    if (read != &egl_context.window) NOT_IMPLEMENTED;
    
    egl_context.current_framebuffer = 0;

    return EGL_TRUE;
}
EGLAPI EGLBoolean EGLAPIENTRY eglQueryContext (EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint *value) { return 0; }
EGLAPI const char *EGLAPIENTRY eglQueryString (EGLDisplay dpy, EGLint name) { return 0; }
EGLAPI EGLBoolean EGLAPIENTRY eglQuerySurface (EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint *value) { return 0; }


GL_APICALL void GL_APIENTRY glEGLReadnPixels (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, GLboolean swap_rb, GLboolean swap_y, void *data);

EGLAPI EGLBoolean EGLAPIENTRY eglSwapBuffers (EGLDisplay dpy, EGLSurface surface) 
{ 
    if (dpy != egl_context.display) NOT_IMPLEMENTED;
    if (surface != &egl_context.window) NOT_IMPLEMENTED;

    glEGLReadnPixels(0,0,egl_context.width, egl_context.height, GL_RGBA, GL_UNSIGNED_BYTE, egl_context.width*egl_context.height*4, 1, 1, egl_context.pixels);

    XPutImage(egl_context.display,
              egl_context.window,
              egl_context.gc,
              egl_context.image,
              0, 0,
              0, 0,
              egl_context.width, egl_context.height);

    XFlush(egl_context.display);

    return EGL_TRUE; 
}

EGLAPI EGLBoolean EGLAPIENTRY eglTerminate (EGLDisplay dpy) { return 0; }
EGLAPI EGLBoolean EGLAPIENTRY eglWaitGL (void) { return 0; }
EGLAPI EGLBoolean EGLAPIENTRY eglWaitNative (EGLint engine) { return 0; }


EGLAPI EGLBoolean EGLAPIENTRY eglBindTexImage (EGLDisplay dpy, EGLSurface surface, EGLint buffer) { return 0; }
EGLAPI EGLBoolean EGLAPIENTRY eglReleaseTexImage (EGLDisplay dpy, EGLSurface surface, EGLint buffer) { return 0; }
EGLAPI EGLBoolean EGLAPIENTRY eglSurfaceAttrib (EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint value) { return 0; }
EGLAPI EGLBoolean EGLAPIENTRY eglSwapInterval (EGLDisplay dpy, EGLint interval) { return 0; }


EGLAPI EGLBoolean EGLAPIENTRY eglBindAPI (EGLenum api) 
{
    if (EGL_OPENGL_API)
    {
        return EGL_TRUE;
    } 
    
    NOT_IMPLEMENTED;
}
EGLAPI EGLenum EGLAPIENTRY eglQueryAPI (void) { return 0; }
EGLAPI EGLSurface EGLAPIENTRY eglCreatePbufferFromClientBuffer (EGLDisplay dpy, EGLenum buftype, EGLClientBuffer buffer, EGLConfig config, const EGLint *attrib_list) { return 0; }
EGLAPI EGLBoolean EGLAPIENTRY eglReleaseThread (void) { return 0; }
EGLAPI EGLBoolean EGLAPIENTRY eglWaitClient (void) { return 0; }


EGLAPI EGLContext EGLAPIENTRY eglGetCurrentContext (void) { return 0; }


EGLAPI EGLSync EGLAPIENTRY eglCreateSync (EGLDisplay dpy, EGLenum type, const EGLAttrib *attrib_list) { return 0; }
EGLAPI EGLBoolean EGLAPIENTRY eglDestroySync (EGLDisplay dpy, EGLSync sync) { return 0; }
EGLAPI EGLint EGLAPIENTRY eglClientWaitSync (EGLDisplay dpy, EGLSync sync, EGLint flags, EGLTime timeout) { return 0; }
EGLAPI EGLBoolean EGLAPIENTRY eglGetSyncAttrib (EGLDisplay dpy, EGLSync sync, EGLint attribute, EGLAttrib *value) { return 0; }
EGLAPI EGLImage EGLAPIENTRY eglCreateImage (EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLAttrib *attrib_list) { return 0; }
EGLAPI EGLBoolean EGLAPIENTRY eglDestroyImage (EGLDisplay dpy, EGLImage image) { return 0; }
EGLAPI EGLDisplay EGLAPIENTRY eglGetPlatformDisplay (EGLenum platform, void *native_display, const EGLAttrib *attrib_list) { return 0; }
EGLAPI EGLSurface EGLAPIENTRY eglCreatePlatformWindowSurface (EGLDisplay dpy, EGLConfig config, void *native_window, const EGLAttrib *attrib_list) { return 0; }
EGLAPI EGLSurface EGLAPIENTRY eglCreatePlatformPixmapSurface (EGLDisplay dpy, EGLConfig config, void *native_pixmap, const EGLAttrib *attrib_list) { return 0; }
EGLAPI EGLBoolean EGLAPIENTRY eglWaitSync (EGLDisplay dpy, EGLSync sync, EGLint flags) { return 0; }