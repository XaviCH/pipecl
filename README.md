# PipeCL

PipeCL is a lightweight rasterizer written in OpenCL. The backend is implemented on top of OpenCL and implements OpenGL Safety Critical 2.0 API.

## Building

This section contains instructions for building PoCL in its default configuration and a subset of driver backends. You can find the full build instructions including a list of available options in the install guide.

### Requirements

In order to build PipeCL, you need the following support libraries and
tools:

- `gcc`, `make` and `xxd`.
- Optional: for building tests/apps `libglm-dev`.

For building all the tools and libraries run:

```bash
git clone https://github.com/XaviCH/pipecl.git
cd pipecl
make
```

The repositories provides:

- `clc`: OpenCL compiler. Usage: `./clc <src> <dst> <flags...>`.
- `glslc`: GLSL offline compiler. Usage: `./glslc <src> <dst> <flags...>`.
- `libGLSC2.so`: OpenGL Safety Critical 2.0 implementation.
- `libEGL.so`: EGL implementation for GLSC2 driver.
