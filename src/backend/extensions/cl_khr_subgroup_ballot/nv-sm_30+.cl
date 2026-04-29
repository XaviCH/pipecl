#ifndef NVIDIA_SM_VERSION
    #error NVIDIA_SM_VERSION required to use this extension.
#endif

#if NVIDIA_SM_VERSION <= 0
    #error NVIDIA_SM_VERSION is not supported
#endif


static inline uint __attribute__((overloadable)) sub_group_non_uniform_broadcast (uint value, uint index) 
{
    uint r;
    __asm__ volatile(
        "{"
        ".reg .b32 mask;"
        "activemask.b32 mask;"
        "shfl.sync.idx.b32  %0, %1, %2, 0x1f, mask;"
        "}"
        : "=r"(r) : "r"(value), "r"(index));
    return r;
}

static inline uint __attribute__((overloadable)) sub_group_broadcast_first (uint value) 
{
    uint r;
    __asm__ volatile(
        "{"
        ".reg .b32 mask, rmask;"
        ".reg .u32 id;"
        "activemask.b32 mask;"
        "brev.b32  rmask, mask;"
        "bfind.u32 id, rmask;"
        "shfl.sync.idx.b32  %0, %1, id, 0x1f, mask;"
        "}"
        : "=r"(r) : "r"(value));
    return r;
}

static inline uint4 sub_group_ballot(int predicate) 
{
    uint r; 
    __asm__ volatile(
        "{"
        ".reg .pred p;"
        ".reg .b32 mask;"
        "setp.ne.u32 p, %1, 0;"
        "activemask.b32 mask;"
        "vote.sync.ballot.b32 %0, p, mask;"
        "}"
        : "=r"(r) : "r"(predicate)
    ); 
    return (uint4)(r,0,0,0); 
}