#ifdef __COMPILER_RELATIVE_PATH__
    #include <constants.device.h>
    #include <backend/types.cl>
#else
    #include "glsc2/src/constants.device.h"
    #include "glsc2/src/backend/types.cl"
#endif


inline uint get_clear_color(const ulong c_clear_write_values) {
    return (uint)(c_clear_write_values & 0xFFFFFFFFu);
}

inline ushort get_clear_depth(const ulong c_clear_write_values) {
    return (ushort)((c_clear_write_values >> 32) & 0xFFFFu);
}

inline uchar get_clear_stencil(const ulong c_clear_write_values) {
    return (uchar)((c_clear_write_values >> 48) & 0xFFu);
}

inline uchar get_clear_stencil_mask(const ushort c_clear_enabled_data) {
    return (uchar)(c_clear_enabled_data & 0xFFu);
} 

static inline uint clear_color(
    const uint color, 
    const ulong clear_write_values, 
    const enabled_data_t enabled_data
) {
    uint clear_color = get_clear_color(clear_write_values);

    uint clear_mask = 
        ((get_enabled_red_data   (enabled_data) * 0xFFu) <<  0) |
        ((get_enabled_green_data (enabled_data) * 0xFFu) <<  8) |
        ((get_enabled_blue_data  (enabled_data) * 0xFFu) << 16) |
        ((get_enabled_alpha_data (enabled_data) * 0xFFu) << 25) ;
    
    return (color & ~clear_mask) | (clear_color & clear_mask);
}

static inline ushort clear_depth(
    const ushort depth, 
    const ulong clear_write_values, 
    const enabled_data_t enabled_data
) {
    return get_enabled_depth_data(enabled_data) ? 
        get_clear_depth(clear_write_values) : depth;
}

static inline uchar clear_stencil(
    const uchar stencil, 
    const ulong clear_write_values, 
    const enabled_data_t enabled_data
) {
    uchar clear_stencil = get_clear_stencil(clear_write_values);

    uchar clear_mask = get_enabled_stencil_data(enabled_data);
    
    return (stencil & ~clear_mask) | (clear_stencil & clear_mask);
}