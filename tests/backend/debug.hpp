#include <stdio.h>
#include <CL/opencl.h>

#define CL_CHECK(_expr)                                                \
   do {                                                                \
     cl_int _err = _expr;                                              \
     if (_err == CL_SUCCESS)                                           \
       break;                                                          \
     printf("OpenCL error at %s:%d. %s returned %d!\n", __FILE__, __LINE__, #_expr, (int)_err);   \
     exit(-1);                                                         \
   } while (0)

#define CL_CHECK2(_expr)                                               \
   ({                                                                  \
     cl_int _err = CL_INVALID_VALUE;                                   \
     decltype(_expr) _ret = _expr;                                     \
     if (_err != CL_SUCCESS) {                                         \
       printf("OpenCL error at %s:%d. %s returned %d!\n", __FILE__, __LINE__, #_expr, (int)_err); \
       exit(-1);                                                       \
     }                                                                 \
     _ret;                                                             \
   })
   