#ifdef __COMPILER_RELATIVE_PATH__
  #include <constants.device.h>
#else
  #include "glsc2/src/constants.device.h"
#endif

inline uint get_stencil_func  (uint c_stencil_data) { return (c_stencil_data >>  0) & 0xFu; }
inline uint get_stencil_operation_sfail    (uint c_stencil_data) { return (c_stencil_data >>  4) & 0xFu; }
inline uint get_stencil_operation_dpass    (uint c_stencil_data) { return (c_stencil_data >>  8) & 0xFu; }
inline uint get_stencil_operation_dfail    (uint c_stencil_data) { return (c_stencil_data >>  12) & 0xFu; }
inline uchar get_stencil_mask  (uint c_stencil_data) { return (c_stencil_data >> 16) & 0xFFu; }
inline uchar get_stencil_ref   (uint c_stencil_data) { return (c_stencil_data >> 24) & 0xFFu; }


bool stencil_test(uchar stencil, uint c_stencil_data) {
  uint func   = get_stencil_func  (c_stencil_data);
  uchar ref   = get_stencil_ref   (c_stencil_data);
  uchar mask  = get_stencil_mask  (c_stencil_data);

  switch(func) {
    default:
    case STENCIL_FUNC_NEVER:
      return false;
    case STENCIL_FUNC_LESS:
      return (mask & ref) < (mask & stencil);
    case STENCIL_FUNC_EQUAL:
      return (mask & ref) == (mask & stencil);
    case STENCIL_FUNC_LEQUAL:
      return (mask & ref) <= (mask & stencil);
    case STENCIL_FUNC_GREATER:
      return (mask & ref) > (mask & stencil);
    case STENCIL_FUNC_NOTEQUAL:
      return (mask & ref) != (mask & stencil);
    case STENCIL_FUNC_GEQUAL:
      return (mask & ref) >= (mask & stencil);
    case STENCIL_FUNC_ALWAYS:
      return true;
  }
}


inline void stencil_operation(local volatile uchar* stencil_buf, uint c_stencil_data, uint operation) {
  uchar ref   = get_stencil_ref   (c_stencil_data);
  uchar mask  = get_stencil_mask  (c_stencil_data);
  
  switch(operation) {
    case STENCIL_OP_KEEP:
      return;
    case STENCIL_OP_ZERO:
      *stencil_buf = (*stencil_buf & ~mask) | (0 & mask);
      return;
    case STENCIL_OP_REPLACE:
      *stencil_buf = (*stencil_buf & ~mask) | (ref & mask);
      return;
    case STENCIL_OP_INCR:
      if (*stencil_buf < 0xFFu) *stencil_buf = (*stencil_buf & ~mask) | ((*stencil_buf + 1) & mask);
      return;
    case STENCIL_OP_DECR:
      if (*stencil_buf > 0x00u) *stencil_buf = (*stencil_buf & ~mask) | ((*stencil_buf - 1) & mask);
      else *stencil_buf = mask & (*stencil_buf);
      return;
    case STENCIL_OP_INVERT:
      *stencil_buf = (*stencil_buf & ~mask) | (~(*stencil_buf) & mask);
      return;
    case STENCIL_OP_INCR_WRAP:
      *stencil_buf = (*stencil_buf & ~mask) | ((*stencil_buf + 1) & mask);
      return;
    case STENCIL_OP_DECR_WRAP:
      *stencil_buf = (*stencil_buf & ~mask) | ((*stencil_buf - 1) & mask);
      return;
  }
}
