#ifndef BACKEND_UTILS_COMMON_CL
#define BACKEND_UTILS_COMMON_CL

#include <constants.device.h>
#include <types.device.h>

//-----------------------------------------------------------------------------

// Those are always mapped into kernel dimensions.
inline uint get_sub_group_local_id()    { return get_local_id(0);   }
inline uint get_sub_group_id()          { return get_local_id(1);   }
inline uint get_sub_group_size()        { return get_local_size(0); }
inline uint get_num_sub_groups()        { return get_local_size(1); }

// Utility functions

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

#if __OPENCL_VERSION__ < 200
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

// UTILS

//------------------------------------------------------------------------

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
    ulong mask = *(local ulong*)((local uchar*)lut + oct + (shape << 5));

    // Second half.

    add_add_carry(&yfrac, yfrac, yinc, &shape, shape, shape);
    shape = add_clamp_0_x(ugetHi(yinit) + 4, popcount(shape & 15), 11);
    add_add_carry(&yfrac, yfrac, yinc, &shape, shape, shape);
    add_add_carry(&yfrac, yfrac, yinc, &shape, shape, shape);
    add_add_carry(&yfrac, yfrac, yinc, &shape, shape, shape);
    mask |= *(local ulong*)((local uchar*)lut + oct + (shape << 5) + (12 << 8));
    return (flips >= (1 << CR_FLIPBIT_COMPL)) ? ~mask : mask;
}

static inline void cover8x8_setupLUT(local volatile ulong* lut)
{
    for (int lutIdx = get_local_linear_id(); lutIdx < CR_COVER8X8_LUT_SIZE; lutIdx += get_local_linear_size())
    {
        int _half       = (lutIdx < (CR_COVER8X8_LUT_SIZE/2)) ? 0 : 1;
        int yint       = (lutIdx >> 5) - _half * 12 - 3;
        uint shape      = ((lutIdx >> 2) & 7) << (31 - 2);
        int slctSwapXY = lutIdx << (31 - 1);
        int slctNegX   = lutIdx << (31 - 0);
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
    long yinit = convert_long(slct_f(yzero, -yzero, slctFlipY));
    uint yinc  = convert_uint_sat_rte((float)abs(slct_i(dy, dx, slctSwapXY)) * xrcp * yincScale);

    // Lookup.

    return cover8x8_lookup_mask(yinit, yinc, flips, lut);
}

static inline uint idiv_fast(uint a, uint b)
{
    return convert_uint_sat_rtn(((float)a + 0.5f) / (float)b);
}

//------------------------------------------------------------------------
// v0 = subpixels relative to the bottom-left sampling point

static inline int float_to_bits(float value) { return *(int*) &value; }

static inline uint3 setupPleq(float3 values, int2 v0, int2 d1, int2 d2, float areaRcp, int samplesLog2)
{
    float mx = fmax(fmax(values.x, values.y), values.z);
    int sh = min(max((float_to_bits(mx) >> 23) - (127 + 22), 0), 8);
    int t0 = (uint)values.x >> sh;
    int t1 = ((uint)values.y >> sh) - t0;
    int t2 = ((uint)values.z >> sh) - t0;
    
    uint rcpMant = (float_to_bits(areaRcp) & 0x007FFFFF) | 0x00800000;
    int rcpShift = (23 + 127) - (float_to_bits(areaRcp) >> 23);

    uint3 pleq;
    long xc = ((long)t1 * d2.y - (long)t2 * d1.y) * rcpMant;
    long yc = ((long)t2 * d1.x - (long)t1 * d2.x) * rcpMant;
    pleq.x = (uint)(xc >> (rcpShift - (sh + CR_SUBPIXEL_LOG2 - samplesLog2)));
    pleq.y = (uint)(yc >> (rcpShift - (sh + CR_SUBPIXEL_LOG2 - samplesLog2)));

    int centerX = (v0.x * 2 + min_min(d1.x, d2.x, 0) + max_max(d1.x, d2.x, 0)) >> (CR_SUBPIXEL_LOG2 - samplesLog2 + 1);
    int centerY = (v0.y * 2 + min_min(d1.y, d2.y, 0) + max_max(d1.y, d2.y, 0)) >> (CR_SUBPIXEL_LOG2 - samplesLog2 + 1);
    int vcx = v0.x - (centerX << (CR_SUBPIXEL_LOG2 - samplesLog2));
    int vcy = v0.y - (centerY << (CR_SUBPIXEL_LOG2 - samplesLog2));

    pleq.z = t0 << sh;
    pleq.z -= (uint)(((xc >> 13) * vcx + (yc >> 13) * vcy) >> (rcpShift - (sh + 13)));
    pleq.z -= pleq.x * centerX + pleq.y * centerY;
    return pleq;
}

//------------------------------------------------------------------------

static inline int clipPolygonWithPlane(float* baryOut, const float* baryIn, int numIn, float v0, float v1, float v2)
{
    int numOut = 0;
    if (numIn >= 3)
    {
        int ai = (numIn - 1) * 2;
        float av = v0 + v1 * baryIn[ai + 0] + v2 * baryIn[ai + 1];
        for (int bi = 0; bi < numIn * 2; bi += 2)
        {
            float bv = v0 + v1 * baryIn[bi + 0] + v2 * baryIn[bi + 1];
            if (av * bv < 0.0f)
            {
                float bc = av / (av - bv);
                float ac = 1.0f - bc;
                baryOut[numOut + 0] = baryIn[ai + 0] * ac + baryIn[bi + 0] * bc;
                baryOut[numOut + 1] = baryIn[ai + 1] * ac + baryIn[bi + 1] * bc;
                numOut += 2;
            }
            if (bv >= 0.0f)
            {
                baryOut[numOut + 0] = baryIn[bi + 0];
                baryOut[numOut + 1] = baryIn[bi + 1];
                numOut += 2;
            }
            ai = bi;
            av = bv;
        }
    }
    return (numOut >> 1);
}
//------------------------------------------------------------------------
// bary = &Vec2f[9] (output)
// v0 = &Vec4f(clipPos0)
// v1 = &Vec4f(clipPos1)
// v2 = &Vec4f(clipPos2)
// d1 = &Vec4f(clipPos1 - clipPos0)
// d2 = &Vec4f(clipPos2 - clipPos0)

static inline int clipTriangleWithFrustum(float* bary, const float* v0, const float* v1, const float* v2, const float* d1, const float* d2)
{
    int num = 3;
    bary[0] = 0.0f, bary[1] = 0.0f;
    bary[2] = 1.0f, bary[3] = 0.0f;
    bary[4] = 0.0f, bary[5] = 1.0f;

    if ((v0[3] < fabs(v0[0])) | (v1[3] < fabs(v1[0])) | (v2[3] < fabs(v2[0])))
    {
        float temp[18];
        num = clipPolygonWithPlane(temp, bary, num, v0[3] + v0[0], d1[3] + d1[0], d2[3] + d2[0]);
        num = clipPolygonWithPlane(bary, temp, num, v0[3] - v0[0], d1[3] - d1[0], d2[3] - d2[0]);
    }
    if ((v0[3] < fabs(v0[1])) | (v1[3] < fabs(v1[1])) | (v2[3] < fabs(v2[1])))
    {
        float temp[18];
        num = clipPolygonWithPlane(temp, bary, num, v0[3] + v0[1], d1[3] + d1[1], d2[3] + d2[1]);
        num = clipPolygonWithPlane(bary, temp, num, v0[3] - v0[1], d1[3] - d1[1], d2[3] - d2[1]);
    }
    if ((v0[3] < fabs(v0[2])) | (v1[3] < fabs(v1[2])) | (v2[3] < fabs(v2[2])))
    {
        float temp[18];
        num = clipPolygonWithPlane(temp, bary, num, v0[3] + v0[2], d1[3] + d1[2], d2[3] + d2[2]);
        num = clipPolygonWithPlane(bary, temp, num, v0[3] - v0[2], d1[3] - d1[2], d2[3] - d2[2]);
    }
    return num;
}

#endif