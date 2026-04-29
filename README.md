# PipeCL

PipeCL is a lightweight open-source rasterizer written in OpenCL. The backend is implemented on top of OpenCL and it is exposed using OpenGL Safety Critical 2.0 API.

---

## Build

The build systems uses make tool.

```bash
make
```

---

## Offline shader compiler 

For compiling offline shaders, glslc tool is provided to generate binary shaders.

```bash
./glslc <src> <dst> <flags...>
```
