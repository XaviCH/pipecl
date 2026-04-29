#ifdef __COMPILER_RELATIVE_PATH__
  #include <backend/utils/sync.cl>
  #include <constants.device.h>
#else
  #include "glsc2/src/backend/utils/sync.cl"
  #include "glsc2/src/constants.device.h"
#endif

inline uint get_depth_func  (uint c_depth_data) { return (c_depth_data >> 0) & 0xFFFFu; }
inline bool get_depth_mask  (uint c_depth_data) { return (c_depth_data >> 16) != 0; }

inline bool depth_test(ushort z, ushort depth, uint c_depth_data) {

  uint func = get_depth_func(c_depth_data);

  switch(func) {
    default:
    case DEPTH_FUNC_NEVER:
      return false;
    case DEPTH_FUNC_LESS:
      return z <  depth;
    case DEPTH_FUNC_EQUAL:
      return z == depth;
    case DEPTH_FUNC_LEQUAL:
      return z <= depth;
    case DEPTH_FUNC_GREATER:
      return z > depth;
    case DEPTH_FUNC_NOTEQUAL:
      return z != depth;
    case DEPTH_FUNC_GEQUAL:
      return z >= depth;
    case DEPTH_FUNC_ALWAYS:
      return true;
  }

}

inline bool update_tile_z(ushort depth, ushort tile_z, uint c_depth_data) {
  // return depth == tile_z;
  uint func = get_depth_func(c_depth_data);
  uint mask = get_depth_mask(c_depth_data);

  if (!mask) return false;

  switch(func) {
    default:
    case DEPTH_FUNC_NEVER:
    case DEPTH_FUNC_EQUAL:
      return false;
    case DEPTH_FUNC_LESS:
    case DEPTH_FUNC_LEQUAL:
    case DEPTH_FUNC_GREATER:
    case DEPTH_FUNC_NOTEQUAL:
    case DEPTH_FUNC_GEQUAL:
    case DEPTH_FUNC_ALWAYS:
      return depth == tile_z;
  }
  
};


inline bool range_depth_test(ushort z, ushort min_depth, ushort max_depth, uint c_depth_data) {
  uint func = get_depth_func(c_depth_data);

  switch(func) {
    default:
    case DEPTH_FUNC_NEVER:
      return false;
    case DEPTH_FUNC_LESS:
      return z <  max_depth;
    case DEPTH_FUNC_EQUAL:
      return z >= min_depth && z <= max_depth;
    case DEPTH_FUNC_LEQUAL:
      return z <= max_depth;
    case DEPTH_FUNC_GREATER:
      return z >  min_depth;
    case DEPTH_FUNC_NOTEQUAL:
      return z <  min_depth && z > max_depth;
    case DEPTH_FUNC_GEQUAL:
      return z >= min_depth;
    case DEPTH_FUNC_ALWAYS:
      return true;
  }

}


inline void init_tile_z_max(ushort* tile_z_max, bool* tile_z_upd, local volatile ushort* w_tile_depth)
{
  *tile_z_max = CR_DEPTH_MAX >> 16;
  *tile_z_upd = (min(w_tile_depth[get_local_id(0)], w_tile_depth[get_local_id(0) + 32]) < *tile_z_max);
}

inline void init_tile_z_min(ushort* tile_z_min, bool* tile_z_upd, local volatile ushort* w_tile_depth)
{
  *tile_z_min = CR_DEPTH_MIN >> 16;
  *tile_z_upd = (max(w_tile_depth[get_local_id(0)], w_tile_depth[get_local_id(0) + 32]) > *tile_z_min);
}

#ifdef DEVICE_SUB_GROUP_INTRINSICTS_ENABLED
inline void sub_group_update_tile_z_max(uint c_render_mode_flags, ushort* tile_z_max, bool* tile_z_upd, local volatile ushort* w_tile_depth)
{
    if ((c_render_mode_flags & RENDER_MODE_FLAG_ENABLE_DEPTH) != 0 && sub_group_any(*tile_z_upd))
    {
        ushort z = max(w_tile_depth[get_local_id(0)], w_tile_depth[get_local_id(0) + 32]);
        *tile_z_max = sub_group_reduce_max(z);
        *tile_z_upd = false;
    }
}
inline void sub_group_update_tile_z_min(uint c_render_mode_flags, ushort* tile_z_min, bool* tile_z_upd, local volatile ushort* w_tile_depth)
{
    if ((c_render_mode_flags & RENDER_MODE_FLAG_ENABLE_DEPTH) != 0 && sub_group_any(*tile_z_upd))
    {
        ushort z = min(w_tile_depth[get_local_id(0)], w_tile_depth[get_local_id(0) + 32]);
        *tile_z_min = sub_group_reduce_min(z);
        *tile_z_upd = false;
    }
}
#endif

inline void local_1dim_update_tile_z_max(uint c_render_mode_flags, ushort* tile_z_max, bool* tile_z_upd, local volatile ushort* w_tile_depth, local volatile uint* l_temp)
{
    if ((c_render_mode_flags & RENDER_MODE_FLAG_ENABLE_DEPTH) != 0) {
        ushort z = max(w_tile_depth[get_local_id(0)], w_tile_depth[get_local_id(0) + 32]);
        // *tile_z_max = local_reduce_max_1dim_ui(z, l_temp);
        *tile_z_upd = false;
    }
}

inline void local_1dim_update_tile_z_min(uint c_render_mode_flags, ushort* tile_z_min, bool* tile_z_upd, local volatile ushort* w_tile_depth, local volatile uint* l_temp)
{
    if ((c_render_mode_flags & RENDER_MODE_FLAG_ENABLE_DEPTH) != 0) {
        ushort z = min(w_tile_depth[get_local_id(0)], w_tile_depth[get_local_id(0) + 32]);
        // *tile_z_min = local_1dim_reduce_min_ui(z, l_temp);
        *tile_z_upd = false;
    }
}