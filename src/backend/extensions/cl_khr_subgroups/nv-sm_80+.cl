#ifndef NVIDIA_SM_VERSION
    #error NVIDIA_SM_VERSION required to use this extension.
#endif

#if NVIDIA_SM_VERSION <= 0
    #error NVIDIA_SM_VERSION is not supported
#endif

// Built-in Sub-Group Synchronization Functions

static inline void __attribute__((overloadable)) sub_group_barrier(cl_mem_fence_flags flags) 
{
    __asm__ volatile("bar.warp.sync 0xffffffff;");
}

#if __OPENCL_VERSION__ >= 200
static inline void __attribute__((overloadable)) sub_group_barrier(cl_mem_fence_flags flags, memory_scope scope)
{
    sub_group_barrier(flags);
}
#endif

// Built-in Sub-Group Collective Functions

static inline uint sub_group_all(int p) 
{
    uint r; 
    
    __asm__ volatile(
        "{"
        ".reg .pred pi;"
        "setp.ne.u32 pi, %1, 0;"
        "vote.sync.all.pred  pi, pi, 0xffffffff;"
        "selp.u32 %0, 1, 0, pi;"
        "}"
        : "=r"(r) : "r"(p)); 

    return r; 
}

static inline uint sub_group_any(int p) 
{
    uint r; 
    
    __asm__ volatile(
        "{"
        ".reg .pred pi;"
        "setp.ne.u32 pi, %1, 0;"
        "vote.sync.any.pred  pi, pi, 0xffffffff;"
        "selp.u32 %0, 1, 0, pi;"
        "}"
        : "=r"(r) : "r"(p)); 

    return r; 
}


static inline uint __attribute__((overloadable)) sub_group_broadcast (uint x, uint sub_group_local_id) 
{
    uint r;
    
    __asm__ volatile(
        "shfl.sync.idx.b32  %0, %1, %2, 0x1f, 0xffffffff;"
        : "=r"(r) : "r"(x), "r"(sub_group_local_id));

    return r;
}

static inline uint __attribute__((overloadable)) sub_group_scan_inclusive_add (uint x) 
{
    uint r;

    __asm__ volatile(
        "{"
        ".reg .pred p;"
        ".reg .b32 dst;"
        "mov.b32 %0, %1;"
        "shfl.sync.up.b32  dst|p, %0, 0x1, 0x0, 0xffffffff;"
        "@p add.u32        %0, dst, %0;"
        "shfl.sync.up.b32  dst|p, %0, 0x2, 0x0, 0xffffffff;"
        "@p add.u32        %0, dst, %0;"
        "shfl.sync.up.b32  dst|p, %0, 0x4, 0x0, 0xffffffff;"
        "@p add.u32        %0, dst, %0;"
        "shfl.sync.up.b32  dst|p, %0, 0x8, 0x0, 0xffffffff;"
        "@p add.u32        %0, dst, %0;"
        "shfl.sync.up.b32  dst|p, %0, 0x10,0x0, 0xffffffff;"
        "@p add.u32        %0, dst, %0;"
        "}"
        : "=r"(r) : "r"(x));

    return r;
}

static inline uint __attribute__((overloadable)) sub_group_scan_inclusive_max (uint x) 
{
    uint r;

    __asm__ volatile(
        "{"
        ".reg .pred p;"
        ".reg .b32 dst;"
        "mov.b32 %0, %1;"
        "shfl.sync.up.b32  dst|p, %0, 0x1, 0x0, 0xffffffff;"
        "@p max.u32        %0, dst, %0;"
        "shfl.sync.up.b32  dst|p, %0, 0x2, 0x0, 0xffffffff;"
        "@p max.u32        %0, dst, %0;"
        "shfl.sync.up.b32  dst|p, %0, 0x4, 0x0, 0xffffffff;"
        "@p max.u32        %0, dst, %0;"
        "shfl.sync.up.b32  dst|p, %0, 0x8, 0x0, 0xffffffff;"
        "@p max.u32        %0, dst, %0;"
        "shfl.sync.up.b32  dst|p, %0, 0x10,0x0, 0xffffffff;"
        "@p max.u32        %0, dst, %0;"
        "}"
        : "=r"(r) : "r"(x));

    return r;
}

static inline uint __attribute__((overloadable)) sub_group_scan_inclusive_min (uint x) 
{
    uint r;
    
    __asm__ volatile(
        "{"
        ".reg .pred p;"
        ".reg .b32 dst;"
        "mov.b32 %0, %1;"
        "shfl.sync.up.b32  dst|p, %0, 0x1, 0x0, 0xffffffff;"
        "@p min.u32        %0, dst, %0;"
        "shfl.sync.up.b32  dst|p, %0, 0x2, 0x0, 0xffffffff;"
        "@p min.u32        %0, dst, %0;"
        "shfl.sync.up.b32  dst|p, %0, 0x4, 0x0, 0xffffffff;"
        "@p min.u32        %0, dst, %0;"
        "shfl.sync.up.b32  dst|p, %0, 0x8, 0x0, 0xffffffff;"
        "@p min.u32        %0, dst, %0;"
        "shfl.sync.up.b32  dst|p, %0, 0x10,0x0, 0xffffffff;"
        "@p min.u32        %0, dst, %0;"
        "}"
        : "=r"(r) : "r"(x));

    return r;
}

static inline uint __attribute__((overloadable)) sub_group_reduce_max (uint x) 
{
    uint r;

    #if NVIDIA_SM_VERSION >= 80
    {
        __asm__ volatile(
            "redux.sync.max.u32 %0, %1, 0xffffffff;"
            : "=r"(r) : "r"(x));
    }
    #else
    {
        r = sub_group_scan_inclusive_max(x);
        r = sub_group_broadcast(r,31);
    }
    #endif
    
    return r;
}

static inline uint __attribute__((overloadable)) sub_group_reduce_min (uint x) 
{
    uint r;

    #if NVIDIA_SM_VERSION >= 80
    {
        __asm__ volatile(
            "redux.sync.min.u32 %0, %1, 0xffffffff;"
            : "=r"(r) : "r"(x));
    }
    #else
    {
        r = sub_group_scan_inclusive_min(x);
        r = sub_group_broadcast(r,31);
    }
    #endif

    return r;
}