#include <compiler/common.h>

#define CL_TARGET_OPENCL_VERSION 120

#include <CL/opencl.h>

#define STRINGIFY(_VAL) #_VAL
#define TO_STRING(_VAL) STRINGIFY(_VAL)

#ifndef INSTALL_PATH
    #error To compile is required to set -DINSTALL_PATH="/path/to/dir" 
#endif

#define SRC_PATH        TO_STRING(INSTALL_PATH) "/src"
#define SHADERS_PATH    TO_STRING(INSTALL_PATH) "/src/backend/shaders"

#define FINE_RASTER_CL  TO_STRING(INSTALL_PATH) "/src/backend/pipeline/fine_raster.cl"

#define CHECK(_ERROR) \
{ \
    if (_ERROR != CL_SUCCESS) {\
        printf("Unexpected OpenCL error (%d | %x) on %s:%d.\n", _ERROR, _ERROR, __FILE__, __LINE__);\
        exit(1);\
    }\
}

cl_int error;
cl_platform_id platform_id;
cl_device_id device_id;
cl_context context;

// args
char** input_files;
char* output_file;
char** options;

void parse_args(int argc, char** argv);

int main(int argc, char** argv) {

    // choose platform
    const cl_uint num_platform_entries = DEVICE_PLATFORM_ID + 1;
    
    cl_uint num_platforms;
    cl_int platform_error;
    cl_platform_id platform_entry_ids[num_platform_entries];
    platform_error = clGetPlatformIDs(num_platform_entries, platform_entry_ids, &num_platforms);
    
    if (platform_error) 
    {
        switch (platform_error)
        {
        case CL_PLATFORM_NOT_FOUND_KHR:
            fprintf(stderr, "No platform found. CL_PLATFORM_NOT_FOUND_KHR %d.\n", platform_error);
        default:
            fprintf(stderr, "Unexepected error %d.\n", platform_error);
        }
        exit(platform_error);
    }

    platform_id = platform_entry_ids[DEVICE_PLATFORM_ID];

    // List available platforms    
    cl_platform_id* platform_ids = NULL;
    if (num_platforms > 1)
        platform_ids = malloc(num_platforms*sizeof(cl_platform_id));
    else
        platform_ids = &platform_id;

    printf("Platforms Available.\n");
    for (cl_uint platform = 0; platform < num_platforms; ++platform) {
        size_t param_value_size_ret;
        clGetPlatformInfo(platform_ids[platform], CL_PLATFORM_NAME, 0, NULL, &param_value_size_ret);
        char* name = malloc(param_value_size_ret);
        clGetPlatformInfo(platform_ids[platform], CL_PLATFORM_NAME, param_value_size_ret, name, NULL);

        clGetPlatformInfo(platform_ids[platform], CL_PLATFORM_VENDOR, 0, NULL, &param_value_size_ret);
        char* vendor = malloc(param_value_size_ret);
        clGetPlatformInfo(platform_ids[platform], CL_PLATFORM_VENDOR, param_value_size_ret, vendor, NULL);

        clGetPlatformInfo(platform_ids[platform], CL_PLATFORM_VERSION, 0, NULL, &param_value_size_ret);
        char* version = malloc(param_value_size_ret);
        clGetPlatformInfo(platform_ids[platform], CL_PLATFORM_VERSION, param_value_size_ret, version, NULL);

        printf("\t%s, %s: %s\n", name, vendor, version);

        free(name);
        free(vendor);
        free(version);
    }

    // Print info of selected platform

    size_t param_value_size_ret;
    clGetPlatformInfo(platform_id, CL_PLATFORM_NAME, 0, NULL, &param_value_size_ret);
    char* name = malloc(param_value_size_ret);
    clGetPlatformInfo(platform_id, CL_PLATFORM_NAME, param_value_size_ret, name, NULL);

    clGetPlatformInfo(platform_id, CL_PLATFORM_VENDOR, 0, NULL, &param_value_size_ret);
    char* vendor = malloc(param_value_size_ret);
    clGetPlatformInfo(platform_id, CL_PLATFORM_VENDOR, param_value_size_ret, vendor, NULL);

    clGetPlatformInfo(platform_id, CL_PLATFORM_VERSION, 0, NULL, &param_value_size_ret);
    char* version = malloc(param_value_size_ret);
    clGetPlatformInfo(platform_id, CL_PLATFORM_VERSION, param_value_size_ret, version, NULL);
    
    clGetPlatformInfo(platform_id, CL_PLATFORM_EXTENSIONS, 0, NULL, &param_value_size_ret);
    char* extensions = malloc(param_value_size_ret);
    clGetPlatformInfo(platform_id, CL_PLATFORM_EXTENSIONS, param_value_size_ret, extensions, NULL);

    printf("Compiling in platform %s %s with %s.\nExtensions available: %s.\n", name, vendor, version, extensions);

    free(name);
    free(vendor);
    free(version);
    free(extensions);

    // Choose device
    const cl_uint num_device_entries = DEVICE_DEVICE_ID + 1;
    
    cl_device_id device_entry_ids[num_device_entries];
    error = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_ALL, num_device_entries, device_entry_ids, NULL);
    CHECK(error);

    device_id = device_entry_ids[DEVICE_DEVICE_ID];

    context = clCreateContext(NULL, 1, &device_id, NULL, NULL,  &error);
    CHECK(error);

    uint8_t* file[2];
    size_t file_size[2];

    read_file(argv[1], &file[0], &file_size[0]);

    const char fine_raster_cl_path[] = FINE_RASTER_CL;

    read_file(fine_raster_cl_path, &file[1], &file_size[1]);

    cl_program program = clCreateProgramWithSource(context, 2, (const char**) file, file_size, &error);
    CHECK(error);

    // -cl-nv-verbose
    const char default_options[] = "-DSHADER -D__COMPILER_RELATIVE_PATH__ -cl-kernel-arg-info -I" SHADERS_PATH " -I" SRC_PATH;

    char *options = NULL;
    
    size_t flags_init = 3;
    size_t size = sizeof(default_options); // null terminator is included

    for(size_t flag = flags_init; flag < argc; ++flag)
    {
        size += strlen(argv[flag]) + 1; // + null terminator or space
    }

    options = (char*) malloc(size);
    strcpy(options, default_options);

    size_t offset = sizeof(default_options);
    
    for(size_t flag = flags_init; flag < argc; ++flag)
    {
        options[offset] = ' ';
        offset += 1;
        strcpy(&options[offset], argv[flag]);
        offset += strlen(argv[flag]);
    }

    printf("Compiling with options: %s\n", options);

    // build program

    error = clBuildProgram(program, 1, &device_id, options, NULL, NULL);
    if (error == CL_BUILD_PROGRAM_FAILURE || error) {
        size_t log_size;
        clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);

        // Allocate memory for the log
        char *log = (char *) malloc(log_size);

        // Get the log
        clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, log_size, log, NULL);

        // Print the log
        printf("%s\n", log);
    }
    CHECK(error);

    // retrive build status 
    
    cl_build_status build_status;
    do {
        error = clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_STATUS, sizeof(build_status), &build_status, NULL);
        CHECK(error);
        switch (build_status)
        {
        case CL_BUILD_NONE:
            printf("CL_BUILD_NONE: Unexpected error %s:%d\n", __FILE__, __LINE__);
            exit(build_status);
        case CL_BUILD_IN_PROGRESS:
            printf("CL_BUILD_IN_PROGRESS\n");
            break;
        default:
            break;
        }
    } while(build_status == CL_BUILD_IN_PROGRESS);
    
    // retrieve build info log

    size_t build_log_size;
    error = clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &build_log_size);
    CHECK(error);

    if (build_log_size) 
    {
        char* log = (char*) malloc(build_log_size);
        error = clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, build_log_size, log, NULL);
        CHECK(error);
        if (build_log_size > 2) printf("%s\n", log); // not print empty string, TODO: improve logic.
        free(log);
    }

    switch (build_status)
    {
        case CL_BUILD_ERROR:
            printf("CL_BUILD_ERROR.\n");
            break;
        case CL_BUILD_SUCCESS:
            printf("CL_BUILD_SUCCESS.\n");
            break;
        default:
            printf("ERROR: Unexpected build status value. build_status==%d.\n",(int)build_status);
            exit(build_status);
    }

    if (build_status != CL_BUILD_SUCCESS) exit(1);

    // post compile checks

    size_t num_kernels;
    error = clGetProgramInfo(program, CL_PROGRAM_NUM_KERNELS, sizeof(num_kernels), &num_kernels, NULL);
    CHECK(error);
    
    if (num_kernels == 0) {
        printf("ERROR: No kernel detected on program.\n");
        exit(-1);
    }

    // write binary

    size_t binary_size;

    error = clGetProgramInfo(program, CL_PROGRAM_BINARY_SIZES, sizeof(binary_size), &binary_size, NULL);
    CHECK(error);

    if (binary_size == 0) {
        printf("ERROR: Driver does not support binaries.\n");
        exit(-1);
    }

    unsigned char* binary = (unsigned char*) malloc(binary_size);
    error = clGetProgramInfo(program, CL_PROGRAM_BINARIES, sizeof(binary), &binary, NULL);
    CHECK(error);

    // validate binary

    cl_int binary_status;
    cl_program program_tmp = clCreateProgramWithBinary(context, 1, &device_id, &binary_size, &binary, &binary_status, &error);
    if (error != CL_SUCCESS)
    {
        printf("ERROR: Driver could not load the binary.\n");
        exit(-1);
    }
    
    // output binary

    FILE *output_file = fopen(argv[2], "wb");
    fwrite(binary, sizeof(binary[0]), binary_size, output_file);

    free(binary);
}