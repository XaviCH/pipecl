# clc

CLC_LIBS = -lOpenCL

CLC_TARGETS = src/compiler/clc.o

# glsl

GLSLC_LIBS = -lOpenCL

GLSLC_TARGETS = src/compiler/glslc.o

# egl

EGL_LIBS = -lX11 -lm
EGL_C_FILES = $(wildcard src/egl/*.c) 

LIB_EGL_TARGETS = $(EGL_C_FILES:.c=.o)

# glsc2

GLSC2_LIBS = -lOpenCL
GLSC2_C_FILES = $(wildcard src/frontend/*.c) $(wildcard src/middleware/*.c) 
GLSC2_CL_FILES = $(wildcard src/backend/pipeline/*.cl)

LIB_GLSC2_TARGETS = $(GLSC2_C_FILES:.c=.o) $(GLSC2_CL_FILES:.cl=.o.c)

# --

CFLAGS = -O3

ALL_TARGETS = \
	$(CLC_TARGETS) \
	$(GLSLC_TARGETS) \
	$(LIB_EGL_TARGETS) \
	$(LIB_GLSC2_TARGETS)

BUILD_OBJS = \
	clc \
	glslc \
	libEGL.so \
	libGLSC2.so

all: $(BUILD_OBJS)

.PHONY: all clean clean-all $(ALL_TARGETS)

$(ALL_TARGETS):
	$(MAKE) -C $(dir $@) $(notdir $@)

clc: $(CLC_TARGETS)
	$(CC) $(CFLAGS) -o $@ $^ $(CLC_LIBS)

glslc: $(GLSLC_TARGETS)
	$(CC) $(CFLAGS) -o $@ $^ $(GLSLC_LIBS)

libEGL.so: $(LIB_EGL_TARGETS)
	$(CC) $(CFLAGS) -shared -o $@ $^ $(EGL_LIBS)

libGLSC2.so: $(LIB_GLSC2_TARGETS)
	$(CC) $(CFLAGS) -shared -o $@ $^ $(GLSC2_LIBS)

# todo: maybe could be iterated 
clean:
	$(MAKE) -C ./src clean

clean-all: clean
	$(MAKE) -C ./src clean-all
	rm -f $(BUILD_OBJS)
