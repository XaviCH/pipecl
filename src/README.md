# Code readability

When a header file is meant to be include from device and host code, eg. C code and OpenCL code, it is used *.device.h. 
This means this file is inter compatible between both codes. Also objects and functions must behave exactly in both
executables. This is done to enable reliable communication between host and device.  
