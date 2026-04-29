#ifndef CONFIG_DEVICE_H
#define CONFIG_DEVICE_H

// TODO: support heteregeneous computing

// -----------
// general configuration
// -----------

#define DEVICE_PLATFORM_ID 0

#define DEVICE_DEVICE_ID 0

#define DEVICE_DEBUG_MODE 0
#if (DEVICE_DEBUG_MODE == 0)
    #define NDEBUG
#endif

// -----------
// device architecture description
// -----------

#define DEVICE_NUM_CORES 36

#define DEVICE_LOCAL_MEM_SIZE 0xc000u

#define DEVICE_CONSTANT_MEM_SIZE 0xc000u

#define DEVICE_MEM_BASE_ADDR_ALIGN 512

#define DEVICE_MAX_WORK_GROUP_SIZE 1024

#define DEVICE_SUB_GROUP_THREADS_LOG2 6

#define DEVICE_IMAGE_SUPPORT 0

#define DEVICE_RW_IMAGE_SUPPORT 0

#define DEVICE_BARRIER_SYNC_LOCAL_ATOMIC_SUPPORT 1

// if disable the threads are executed as each local thread is independant of each other
// otherwise local threads operates on sub-groups work
#define DEVICE_SUB_GROUP_SUPPORT 1

// if enabled, sub group threads are executed in locksteps and sub-groups have raw at local memory / global memory.
#define DEVICE_SUB_GROUP_LOCKSTEP_RAW_SUPPORT 0

// if enabled the device supports sub group intra-register operations as ballot, all, any, scan or reduce. 
#define DEVICE_SUB_GROUP_INTRINSICTS_SUPPORT 0

// ------
// render configuration
// ------

#define DEVICE_ORCHESTRATOR_ENABLED 1

#define DEVICE_CONTEXT_NUMBER 1

#define DEVICE_BIN_QUEUE_SIZE 1

#define DEVICE_VERTEX_ATTRIBUTE_SIZE 16
#define DEVICE_UNIFORM_CAPACITY (2*1024) // 2 KB for uniforms

#define DEVICE_VERTEX_TEXTURE_UNITS 0 // max number of vertex texture units
#define DEVICE_TEXTURE_UNITS 8 // max number of active texture units

#define TRIANGLE_PRIMITIVE_CONFIGS_LOG2 5
#define DEVICE_VERTEX_COMMAND_QUEUE_SIZE 8

// max number of triangles could be rasterized
#define DEVICE_MAX_NUMBER_TRIANGLES (1UL << 15) 
#define DEVICE_MAX_NUMBER_SUBTRIANGLES (1UL << 12)

// max number of vertices with varying attributes could be rastired
#define DEVICE_VERTICES_SIZE (1UL << 16)
#define DEVICE_VARYING_SIZE 2

// ------
// kernel configuration
// ------

// vertex shader configuration
#define DEVICE_VERTEX_THREADS (32 * 2)

// triangle setup configuration
#define DEVICE_SETUP_THREADS (32 * 2)

// bin raster configuration
#define DEVICE_BIN_THREADS (32 * 16)

// coarse raster configuration
#define DEVICE_COARSE_THREADS (32 * 16)

// fine raster Configuration
#define DEVICE_MAX_FINE_SUB_GROUPS 20

// ------
// cudaraster configuration // TODO: refactor; this is supose to be configurable, but it isnt nor tested either.
// ------

#define CR_MAXVIEWPORT_LOG2     11      // ViewportSize / PixelSize. Max value for any viewport axis
#define CR_SUBPIXEL_LOG2        4       // PixelSize / SubpixelSize. ??

#define CR_MAXBINS_LOG2         4       // ViewportSize / BinSize. Divide the viewport on x bins
#define CR_BIN_LOG2             4       // BinSize / TileSize.
#define CR_TILE_LOG2            3       // TileSize / PixelSize.

#define CR_COVER8X8_LUT_SIZE    768     // 64-bit entries.
#define CR_FLIPBIT_FLIP_Y       2
#define CR_FLIPBIT_FLIP_X       3
#define CR_FLIPBIT_SWAP_XY      4
#define CR_FLIPBIT_COMPL        5

#define CR_BIN_STREAMS_LOG2     4
#define CR_BIN_SEG_LOG2         9       // 32-bit entries.
#define CR_TILE_SEG_LOG2        5       // 32-bit entries.

#define CR_MAXSUBTRIS_LOG2      24 // 24 reduced to 14     // Triangle structs. Dictated by CoarseRaster.
#define CR_COARSE_QUEUE_LOG2    10      // Triangles.

// ------
// frontend configuration
// ------

#define HOST_PROGRAMS_SIZE 16

#define HOST_BIN_QUEUES_SIZE 8

#define HOST_FRAMEBUFFER_SIZE 16

#define HOST_IMAGES_REF_SIZE 16

#define HOST_BUFFERS_SIZE 128

#define HOST_RENDERBUFFERS_SIZE 8

#define HOST_TEXTURES_SIZE 8

// -----
// software configuration
// -----

#define DEVICE_MAX_TEXTURE_SIZE_LOG2 11 // (2048 x 2048) max size  

#define DEVICE_MIPMAP_LEVELS

// ------
// extensions configuration
// ------

#define USE_CL_KHR_GLOBAL_INT32_BASE_ATOMICS_IMPL 0
#define USE_CL_KHR_LOCAL_INT32_BASE_ATOMICS_IMPL 0
#define USE_CL_KHR_LOCAL_INT32_EXTENDED_ATOMICS_IMPL 0
#define USE_CL_KHR_SUBGROUP_BALLOT_IMPL 1
#define USE_CL_KHR_SUBGROUPS_IMPL 1

// ------
// vendor configuration
// ------

#define NVIDIA_SM_VERSION 89 // 0 if is not an NVIDIA GPU

// some nvidia drivers throw mem exception when using subbuffer, this flag is added in case
#define DEVICE_SUBBUFFER_SUPPORT 1 


#endif