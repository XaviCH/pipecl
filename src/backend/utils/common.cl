#ifndef BACKEND_UTILS_COMMON_CL
#define BACKEND_UTILS_COMMON_CL

#include <constants.device.h>
#include <types.device.h>

// Those are always mapped into kernel dimensions.

#if __OPENCL_C_VERSION__ < 200

static inline size_t get_local_linear_id() 
{ 
    return get_local_id(1) * get_local_size(0) + get_local_id(0); 
}

static inline size_t get_global_linear_id() 
{ 
    return (get_global_id(1) - get_global_offset(1)) * get_global_size(0) 
        +  (get_global_id(0) - get_global_offset(0)); 
}

#endif

static inline size_t get_local_linear_size() 
{
    return get_local_size(0) * get_local_size(1) * get_local_size(2); 
}

// TODO: rm this functions and wrapped around cl_khr_subgroup macro

static inline uint get_sub_group_local_id()    { return get_local_id(0);   }
static inline uint get_sub_group_id()          { return get_local_id(1);   }
static inline uint get_sub_group_size()        { return get_local_size(0); }
static inline uint get_num_sub_groups()        { return get_local_size(1); }

// utility functions

static inline uint ugetLo (ulong a) { return a & 0x00000000FFFFFFFFu; }
static inline uint ugetHi (ulong a) { return a >> 32; }

static inline void add_add_carry(
    uint* rlo, uint alo, uint blo, 
    uint* rhi, uint ahi, uint bhi
) { 
    ulong r = upsample(ahi, alo) + upsample(bhi, blo); 
    *rlo = ugetLo(r); 
    *rhi = ugetHi(r); 
}

static inline int findLeadingOne (uint v) { return 31 - clz(v); }

static inline int add_s16lo_s16lo (int a, int b) { return (short)a + (short)b; }
static inline int add_s16hi_s16lo (int a, int b) { return (a >> 16) + (short)b; }
static inline int sub_s16lo_s16lo (int a, int b) { return (short)a - (short)b; }
static inline int sub_s16hi_s16lo (int a, int b) { return (a >> 16) - (short)b; }
static inline int sub_s16hi_s16hi (int a, int b) { return (a >> 16) - (b >> 16); }

static inline int  max_max       (int a,  int b,  int c)       { return max(a, max(b, c)); }
static inline int  min_min       (int a,  int b,  int c)       { return min(a, min(b, c)); }
static inline uint add_sub       (uint a, uint b, uint c)      { return a+b-c; }
static inline int  add_clamp_0_x (int a,  int b,  int c)       { return clamp(a+b,0,c); }

// TODO: refactor
static inline uint     prmt				(uint a, uint b, uint c)    { 
    ulong tmp = (ulong)b << 32 | a;
    uint4 masks;
    uint v = 0;
    masks.x = (c >>  0) & 0xf;
    masks.y = (c >>  4) & 0xf;
    masks.z = (c >>  8) & 0xf;
    masks.w = (c >> 12) & 0xf;
    
    v |= ((tmp >> (masks.x*8)) & 0xFF) <<  0;
    v |= ((tmp >> (masks.y*8)) & 0xFF) <<  8;
    v |= ((tmp >> (masks.z*8)) & 0xFF) << 16;
    v |= ((tmp >> (masks.w*8)) & 0xFF) << 24;
    
    return v;
}
static inline int      slct_i              (int a, int b, int c)   { return (c >= 0) ? a : b; }
static inline float    slct_f              (float a, float b, int c)   { return (c >= 0) ? a : b; }

static inline uint idiv_fast(uint a, uint b)
{
    return convert_uint_sat_rtn(((float)a + 0.5f) / (float)b);
}



// TODO: high dependence on 8x8 tiles
// cover8x8

static inline uint cover8x8_selectFlips(int dx, int dy) // 10 instr
{
    uint flips = 0;
    if (dy > 0 || (dy == 0 && dx <= 0))
        flips ^= (1 << CR_FLIPBIT_FLIP_X) ^ (1 << CR_FLIPBIT_FLIP_Y) ^ (1 << CR_FLIPBIT_COMPL);
    if (dx > 0)
        flips ^= (1 << CR_FLIPBIT_FLIP_X) ^ (1 << CR_FLIPBIT_FLIP_Y);
    if (abs(dx) < abs(dy))
        flips ^= (1 << CR_FLIPBIT_SWAP_XY) ^ (1 << CR_FLIPBIT_FLIP_Y);
    return flips;
}

static inline ulong cover8x8_lookup_mask(long yinit, uint yinc, uint flips, local volatile const ulong* lut)
{
    // First half.

    uint yfrac = ugetLo(yinit);
    uint shape = add_clamp_0_x(ugetHi(yinit) + 4, 0, 11);
    add_add_carry(&yfrac, yfrac, yinc, &shape, shape, shape);
    add_add_carry(&yfrac, yfrac, yinc, &shape, shape, shape);
    add_add_carry(&yfrac, yfrac, yinc, &shape, shape, shape);
    int oct = flips & ((1 << CR_FLIPBIT_FLIP_X) | (1 << CR_FLIPBIT_SWAP_XY));
    ulong mask = lut[(oct >> 3) + (shape << 2)];

    // Second half.

    add_add_carry(&yfrac, yfrac, yinc, &shape, shape, shape);
    shape = add_clamp_0_x(ugetHi(yinit) + 4, popcount(shape & 15), 11);
    add_add_carry(&yfrac, yfrac, yinc, &shape, shape, shape);
    add_add_carry(&yfrac, yfrac, yinc, &shape, shape, shape);
    add_add_carry(&yfrac, yfrac, yinc, &shape, shape, shape);
    mask |= lut[(oct >> 3) + (shape << 2) + (12 << 5)];
    return (flips >= (1 << CR_FLIPBIT_COMPL)) ? ~mask : mask;
}

static inline void cover8x8_setupLUT(local volatile ulong* lut)
{
    for (uint lutIdx = get_local_linear_id(); lutIdx < CR_COVER8X8_LUT_SIZE; lutIdx += get_local_linear_size())
    {
        int _half       = (lutIdx < (CR_COVER8X8_LUT_SIZE/2)) ? 0 : 1;
        int yint       = (int)(lutIdx >> 5) - _half * 12 - 3;
        uint shape      = ((lutIdx >> 2) & 7) << (31 - 2);
        int slctSwapXY = as_int(lutIdx << (31 - 1));
        int slctNegX   = as_int(lutIdx << (31 - 0));
        int slctCompl  = slctSwapXY ^ slctNegX;

        ulong mask = 0;
        int xlo = _half * 4;
        int xhi = xlo + 4;
        for (int x = xlo; x < xhi; x++)
        {
            int ylo = slct_i(0, max(yint, 0), slctCompl);
            int yhi = slct_i(min(yint, 8), 8, slctCompl);
            for (int y = ylo; y < yhi; y++)
            {
                int xx = slct_i(x, y, slctSwapXY);
                int yy = slct_i(y, x, slctSwapXY);
                xx = slct_i(xx, 7 - xx, slctNegX);
                mask |= (ulong)1 << (xx + yy * 8);
            }
            yint += shape >> 31;
            shape <<= 1;
        }
        lut[lutIdx] = mask;
    }
}

static inline ulong cover8x8_conservative_fast(int ox, int oy, int dx, int dy, uint flips, local volatile const ulong* lut) // 54 instr
{
    float  halfPixel  = (float)(1 << (CR_SUBPIXEL_LOG2 - 1));
    float  yinitBias  = (float)(1 << (31 - CR_MAXVIEWPORT_LOG2 - CR_SUBPIXEL_LOG2 * 2));
    float  yinitScale = (float)(1 << (32 - CR_SUBPIXEL_LOG2));
    float  yincScale  = 65536.0f * 65536.0f;

    int  slctFlipY  = flips << (31 - CR_FLIPBIT_FLIP_Y);
    int  slctFlipX  = flips << (31 - CR_FLIPBIT_FLIP_X);
    int  slctSwapXY = flips << (31 - CR_FLIPBIT_SWAP_XY);

    // Evaluate cross product.

    int t = ox * dy - oy * dx;
    float det = (float)slct_i(t, t - dy * (7 << CR_SUBPIXEL_LOG2), slctFlipX);

    float xabs = (float)abs(slct_i(dx, dy, slctSwapXY));
    float yabs = (float)abs(slct_i(dy, dx, slctSwapXY));
    det = det + xabs * halfPixel + yabs * halfPixel;

    if (flips >= (1 << CR_FLIPBIT_COMPL))
        det = -det;

    // Represent Y as a function of X.

    float xrcp  = 1.0f / xabs;
    float yzero = det * yinitScale * xrcp + yinitBias;
    long yinit = convert_long_rte(slct_f(yzero, -yzero, slctFlipY));
    uint yinc  = convert_uint_sat_rte(yabs * xrcp * yincScale);

    // Lookup.

    return cover8x8_lookup_mask(yinit, yinc, flips, lut);
}

static inline ulong cover8x8_exact_fast(int ox, int oy, int dx, int dy, uint flips, local volatile const ulong* lut) // 52 instr
{
    float  yinitBias  = (float)(1 << (31 - CR_MAXVIEWPORT_LOG2 - CR_SUBPIXEL_LOG2 * 2));
    float  yinitScale = (float)(1 << (32 - CR_SUBPIXEL_LOG2));
    float  yincScale  = 65536.0f * 65536.0f;

    int  slctFlipY  = flips << (31 - CR_FLIPBIT_FLIP_Y);
    int  slctFlipX  = flips << (31 - CR_FLIPBIT_FLIP_X);
    int  slctSwapXY = flips << (31 - CR_FLIPBIT_SWAP_XY);

    // Evaluate cross product.

    int t = ox * dy - oy * dx;
    float det = (float)slct_i(t, t - dy * (7 << CR_SUBPIXEL_LOG2), slctFlipX);
    if (flips >= (1 << CR_FLIPBIT_COMPL))
        det = -det;

    // Represent Y as a function of X.

    float xrcp  = 1.0f / (float) abs(slct_i(dx, dy, slctSwapXY));
    float yzero = det * yinitScale * xrcp + yinitBias;
    long yinit = convert_long_rte(slct_f(yzero, -yzero, slctFlipY));
    uint yinc  = convert_uint_sat_rte((float)abs(slct_i(dy, dx, slctSwapXY)) * xrcp * yincScale);

    // Lookup.

    return cover8x8_lookup_mask(yinit, yinc, flips, lut);
}



#endif // BACKEND_UTILS_COMMON_CL
