#ifdef __COMPILER_RELATIVE_PATH__
  #include <constants.device.h>
#else
  #include "glsc2/src/constants.device.h"
#endif

inline uint get_blending_eq_color   (uint c_blending_data) { return (c_blending_data >>  0) & 0x3u; }
inline uint get_blending_eq_alpha   (uint c_blending_data) { return (c_blending_data >>  2) & 0x3u; }
inline uint get_blending_func_color_src     (uint c_blending_data) { return (c_blending_data >> 16) & 0xfu; }
inline uint get_blending_func_alpha_src     (uint c_blending_data) { return (c_blending_data >> 20) & 0xfu; }
inline uint get_blending_func_color_dst     (uint c_blending_data) { return (c_blending_data >> 24) & 0xfu; }
inline uint get_blending_func_alpha_dst     (uint c_blending_data) { return (c_blending_data >> 28) & 0xfu; }

inline void apply_blend_color_func(uint func, const float4* src, const float4* dst, const float4* color, float4* out) {
  switch (func) {
    default:
    case BLEND_ZERO:
      out->xyz = 0;
      break;
    case BLEND_ONE:
      out->xyz = 1;
      break;
    case BLEND_SRC_COLOR:
      out->xyz = src->xyz;
      break;
    case BLEND_ONE_MINUS_SRC_COLOR:
      out->xyz = 1 - src->xyz;
      break;
    case BLEND_DST_COLOR:
      out->xyz = dst->xyz;
      break;
    case BLEND_ONE_MINUS_DST_COLOR:
      out->xyz = 1 - dst->xyz;
      break;
    case BLEND_SRC_ALPHA:
      out->xyz = src->w;
      break;
    case BLEND_ONE_MINUS_SRC_ALPHA:
      out->xyz = 1 - src->w;
      break;
    case BLEND_DST_ALPHA:
      out->xyz = dst->w;
      break;
    case BLEND_ONE_MINUS_DST_ALPHA:
      out->xyz = 1 - dst->w;
      break;
    case BLEND_CONSTANT_COLOR:
      out->xyz = color->xyz;
      break;
    case BLEND_ONE_MINUS_CONSTANT_COLOR:
      out->xyz = 1 - color->xyz;
      break;
    case BLEND_CONSTANT_ALPHA:
      out->xyz = color->w;
      break;
    case BLEND_ONE_MINUS_CONSTANT_ALPHA:
      out->xyz = 1 - color->w;
      break;
    case BLEND_SRC_ALPHA_SATURATE:
      out->xyz = min(src->w, 1 - dst->w);
      break;
  }
}

inline void apply_blend_alpha_func(uint func, const float4* src, const float4* dst, const float4* color, float4* out) {
  switch (func) {
    default:
    case BLEND_ZERO:
      out->w = 0;
      break;
    case BLEND_ONE:
      out->w = 0;
      break;
    case BLEND_SRC_COLOR:
      out->w = src->w;
      break;
    case BLEND_ONE_MINUS_SRC_COLOR:
      out->w = 1 - src->w;
      break;
    case BLEND_DST_COLOR:
      out->w = dst->w;
      break;
    case BLEND_ONE_MINUS_DST_COLOR:
      out->w = 1 - dst->w;
      break;
    case BLEND_SRC_ALPHA:
      out->w = src->w;
      break;
    case BLEND_ONE_MINUS_SRC_ALPHA:
      out->w = 1 - src->w;
      break;
    case BLEND_DST_ALPHA:
      out->w = dst->w;
      break;
    case BLEND_ONE_MINUS_DST_ALPHA:
      out->w = 1 - dst->w;
      break;
    case BLEND_CONSTANT_COLOR:
      out->w = color->w;
      break;
    case BLEND_ONE_MINUS_CONSTANT_COLOR:
      out->w = 1 - color->w;
      break;
    case BLEND_CONSTANT_ALPHA:
      out->w = color->w;
      break;
    case BLEND_ONE_MINUS_CONSTANT_ALPHA:
      out->w = 1 - color->w;
      break;
    case BLEND_SRC_ALPHA_SATURATE:
      out->w = 1;
      break;
  }
}



inline float4 uint_to_float4(const uint color) {
  #define EXTRACT_BYTE(_REG, _BYTE) ((_REG >> _BYTE*8) & 0xFFu)

  return (float4) {
    (float) EXTRACT_BYTE(color, 0) / 255.f,
    (float) EXTRACT_BYTE(color, 1) / 255.f,
    (float) EXTRACT_BYTE(color, 2) / 255.f,
    (float) EXTRACT_BYTE(color, 3) / 255.f,
  };

  #undef EXTRACT_BYTE
}

static inline uint float4_to_uint(const float4 color) {
  float4 tmp = clamp(color, 0.f, 1.f);

  return 
    (uint)(tmp.x * 255.f) <<  0 |
    (uint)(tmp.y * 255.f) <<  8 |
    (uint)(tmp.z * 255.f) << 16 |
    (uint)(tmp.w * 255.f) << 24 ;
}

inline uint blend(
  uint src, uint dst,
  uint c_blending_color, uint c_blending_data
) {
  float4  srcColor,  dstColor, conColor, wsrcColor, wdstColor, outColor;

  uint blending_func_color_src  = get_blending_func_color_src (c_blending_data);
  uint blending_func_color_dst  = get_blending_func_color_dst (c_blending_data);
  uint blending_func_alpha_src  = get_blending_func_alpha_src (c_blending_data);
  uint blending_func_alpha_dst  = get_blending_func_alpha_dst (c_blending_data);
  uint blending_eq_color    = get_blending_eq_color   (c_blending_data);
  uint blending_eq_alpha    = get_blending_eq_alpha   (c_blending_data);

  srcColor = uint_to_float4(src);
  dstColor = uint_to_float4(dst);
  conColor = uint_to_float4(c_blending_color);

  apply_blend_color_func(blending_func_alpha_src, &srcColor, &dstColor, &conColor, &wsrcColor);
  apply_blend_alpha_func(blending_func_alpha_src, &srcColor, &dstColor, &conColor, &wsrcColor);
  apply_blend_color_func(blending_func_alpha_dst, &srcColor, &dstColor, &conColor, &wdstColor);
  apply_blend_alpha_func(blending_func_alpha_dst, &srcColor, &dstColor, &conColor, &wdstColor);

  switch (blending_eq_color) {
    case BLEND_FUNC_ADD:
      outColor.xyz = srcColor.xyz * wsrcColor.xyz + dstColor.xyz * wdstColor.xyz;
      break;
    case BLEND_FUNC_SUBTRACT:
      outColor.xyz = srcColor.xyz * wsrcColor.xyz - dstColor.xyz * wdstColor.xyz;
      break;
    case BLEND_FUNC_REVERSE_SUBTRACT:
      outColor.xyz = dstColor.xyz * wdstColor.xyz - srcColor.xyz * wsrcColor.xyz;
      break;
  }

  switch (blending_eq_alpha) {
    case BLEND_FUNC_ADD:
      outColor.w = srcColor.w * wsrcColor.w + dstColor.w * wdstColor.w;
      break;
    case BLEND_FUNC_SUBTRACT:
      outColor.w = srcColor.w * wsrcColor.w - dstColor.w * wdstColor.w;
      break;
    case BLEND_FUNC_REVERSE_SUBTRACT:
      outColor.w = dstColor.w * wdstColor.w - srcColor.w * wsrcColor.w;
      break;
  }

  return float4_to_uint(outColor);

}