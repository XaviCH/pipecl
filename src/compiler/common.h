#ifndef COMPILER_COMMON_H
#define COMPILER_COMMON_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <config.device.h>

#define CL_CHECK(...)       \
{                                                                       \
    cl_int __error = __VA_ARGS__;                                             \
    if (__error != CL_SUCCESS) {                                   \
    printf("OpenCL error at %s:%d. " #__VA_ARGS__ " returned %d.\n", __FILE__, __LINE__, (int)__error);    \
    exit(__error);                                                      \
    } \
}

#define CL_ASSIGN_CHECK(_LEFT, ...)                                     \
{                                                                       \
    cl_int error;                                                       \
    _LEFT = __VA_ARGS__;                                                      \
    if (error != CL_SUCCESS) {                                     \
    printf("OpenCL error at %s:%d. " #__VA_ARGS__ " returned %d.\n", __FILE__, __LINE__, (int)error);      \
    exit(error);   \
    }                                                     \
}

static int read_file(const char* filename, uint8_t** data, size_t* size) {
    if (NULL == filename || NULL == data || 0 == size)
        return -1;

    FILE* fp = fopen(filename, "r");
    if (NULL == fp) {
        fprintf(stderr, "Failed to read file %s.", filename);
        return -1;
    }
    fseek(fp , 0 , SEEK_END);
    long fsize = ftell(fp);
    rewind(fp);

    *data = (uint8_t*)malloc(fsize);
    *size = fread(*data, 1, fsize, fp);
  
    fclose(fp);
  
    return 0;
}



#endif // COMPILER_COMMON_H