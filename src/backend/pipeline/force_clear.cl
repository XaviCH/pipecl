#ifdef __COMPILER_RELATIVE_PATH__
    #include <backend/types.cl>
#else
    #include "glsc2/src/backend/types.cl"
#endif


void write_colorbuffer_channels(
    rw_texture2d_t colorbuffer,
    const ulong clear_write_values, 
    const ushort clear_enabled_data
    #ifndef DEVICE_RW_IMAGE_ENABLED
    , const uint colorbuffer_type, const uint buffer_width
    #endif
) {
    ushort enabled_color_channel_mask = (clear_enabled_data & CLEAR_ENABLED_COLOR_CHANNEL_MASK);
    
    if (enabled_color_channel_mask == 0) return;

    size_t x = get_global_id(0);
    size_t y = get_global_id(1);

    uint4 color_value = {
        (clear_write_values >>  0) & 0xFFu,
        (clear_write_values >>  8) & 0xFFu,
        (clear_write_values >> 16) & 0xFFu,
        (clear_write_values >> 24) & 0xFFu
    };

    // Required read
    if (enabled_color_channel_mask != CLEAR_ENABLED_COLOR_CHANNEL_MASK) {
        uint4 buffer_value;
        #ifdef DEVICE_RW_IMAGE_ENABLED
            buffer_value = read_imageui(colorbuffer, (int2){x, y});
        #else
            uint compressed_buffer_value = ((global uchar*)colorbuffer)[x + y*buffer_width];
            buffer_value = (uint4){
                (compressed_buffer_value >>  0) & 0xFFu,
                (compressed_buffer_value >>  8) & 0xFFu,
                (compressed_buffer_value >> 16) & 0xFFu,
                (compressed_buffer_value >> 24) & 0xFFu
            };
        #endif

        color_value = (uint4){
            (enabled_color_channel_mask & CLEAR_ENABLED_COLOR_CHANNEL_RED)    ? color_value.x : buffer_value.x,
            (enabled_color_channel_mask & CLEAR_ENABLED_COLOR_CHANNEL_GREEN)  ? color_value.y : buffer_value.y,
            (enabled_color_channel_mask & CLEAR_ENABLED_COLOR_CHANNEL_BLUE)   ? color_value.z : buffer_value.z,
            (enabled_color_channel_mask & CLEAR_ENABLED_COLOR_CHANNEL_ALPHA)  ? color_value.w : buffer_value.w
        };
    }

    #ifdef DEVICE_RW_IMAGE_ENABLED
        write_imageui(colorbuffer, (int2){x, y}, color_value);
    #else
        ((global uint*)colorbuffer)[x + y*buffer_width] = 
            color_value.x <<  0 |
            color_value.y <<  8 |
            color_value.z << 16 |
            color_value.w << 24 ;
    #endif
}

void write_depthbuffer_(
    rw_texture2d_t depthbuffer,
    const ulong clear_write_values, 
    const ushort clear_enabled_data
    #ifndef DEVICE_RW_IMAGE_ENABLED
    , const uint buffer_width
    #endif
) {
    bool enabled_depth_channel = (clear_enabled_data & CLEAR_ENABLED_DEPTH_CHANNEL) != 0;
    
    if (enabled_depth_channel == 0) return;

    size_t x = get_global_id(0);
    size_t y = get_global_id(1);

    ushort depth_value = (clear_write_values >> 32) & 0xFFFFu;

    #ifdef DEVICE_RW_IMAGE_ENABLED
    write_imageui(depthbuffer, (int2){x, y}, (uint4){depth_value,0,0,0});
    #else
    ((global ushort*)depthbuffer)[x + y*buffer_width] = depth_value;
    #endif
}

void write_stencilbuffer_(
    rw_texture2d_t stencilbuffer,
    const ulong clear_write_values, 
    const ushort clear_enabled_data
    #ifndef DEVICE_RW_IMAGE_ENABLED
    , const uint buffer_width
    #endif
) {
    ushort enabled_stencil_channel_mask = (clear_enabled_data & CLEAR_ENABLED_STENCIL_CHANNEL_MASK);
    
    if (enabled_stencil_channel_mask == 0) return;

    size_t x = get_global_id(0);
    size_t y = get_global_id(1);

    uchar stencil_value = (clear_write_values >> 48) & 0xFFu;

    // Required read
    if (enabled_stencil_channel_mask != CLEAR_ENABLED_STENCIL_CHANNEL_MASK) {
        uchar buffer_value;
        #ifdef DEVICE_RW_IMAGE_ENABLED
            buffer_value = read_imageui(stencilbuffer, (int2){x, y}).x;
        #else
            buffer_value = ((global uchar*)stencilbuffer)[x + y*buffer_width];
        #endif

        uchar bitmask = enabled_stencil_channel_mask >> 0;
        stencil_value = (buffer_value & ~bitmask) | (stencil_value & bitmask);
    }

    #ifdef DEVICE_RW_IMAGE_ENABLED
        write_imageui(stencilbuffer, (int2){x, y}, (uint4){stencil_value,0,0,0});
    #else
        ((global uchar*)stencilbuffer)[x + y*buffer_width] = stencil_value;
    #endif

}

/**
 * Force the clearing of the attached framebuffer.
 *
 * Dimx and dimy is mapped into scissor test values.
 */
kernel void force_clear(
    // framebuffer data
    rw_texture2d_t colorbuffer,
    rw_texture2d_t depthbuffer,
    rw_texture2d_t stencilbuffer,
    #ifndef DEVICE_RW_IMAGE_ENABLED
    const uint colorbuffer_type,
    const uint buffer_width,
    #endif
    const ulong clear_write_values, // 
    const ushort clear_enabled_data // channels available, enableds, ...
) {

    write_colorbuffer_channels(
        colorbuffer, clear_write_values, clear_enabled_data
        #ifndef DEVICE_RW_IMAGE_ENABLED
        , colorbuffer_type, buffer_width
        #endif
    );

    write_depthbuffer_(
        depthbuffer, clear_write_values, clear_enabled_data
        #ifndef DEVICE_RW_IMAGE_ENABLED
        , buffer_width
        #endif
    );

    write_stencilbuffer_(
        stencilbuffer, clear_write_values, clear_enabled_data
        #ifndef DEVICE_RW_IMAGE_ENABLED
        , buffer_width
        #endif
    );

}