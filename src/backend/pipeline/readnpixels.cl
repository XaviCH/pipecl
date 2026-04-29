#include <backend/utils/texture.cl>

kernel void readnpixels(
  colorbuffer_t colorbuffer,
  global void* out,
  const uint colorbuffer_mode,
  const uint out_mode,
  const uchar swap_rb_channels,
  const uchar swap_y_axis
) {
  int2 pos   = {get_global_id(0), get_global_id(1)};
  uint2 size  = {get_global_size(0), get_global_size(1)};

  uint color = read_colorbuffer(colorbuffer, pos, size, colorbuffer_mode);

  if (swap_rb_channels)
  {
    uint red_color  = (color >>  0) & 0xFFu;
    uint blue_color = (color >> 16) & 0xFFu;
    
    color = (color & 0xFF00FF00u) | blue_color | (red_color << 16);
  }

  int2 out_pos = pos;
  if (swap_y_axis)
  {
    out_pos.y = size.y - out_pos.y; 
  }
  
  write_2d_texture_buffer(out, out_pos, size, out_mode, color);
}