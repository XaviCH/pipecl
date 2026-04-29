#ifndef BACKEND_SHADERS_ROP_CL
#define BACKEND_SHADERS_ROP_CL

#ifdef __COMPILER_RELATIVE_PATH__
    #include <backend/types.cl>
    #include <backend/utils/sub_group_mask.cl>
    #include <constants.h>
#else
    #include "glsc2/src/backend/types.cl"
    #include "glsc2/src/backend/utils/sub_group_mask.cl"
    #include "glsc2/src/constants.h"
#endif




#if (DEVICE_SUB_GROUP_RAW == 1)
inline void execute_ROP_single_sample(
    uint render_mode_flags,
    int tri_idx, int pixel_x, int pixel_y,
    fragment_shader_output_t* fs_out, ushort depth, 
    local volatile uint* ptr_color, local volatile ushort* ptr_depth, local volatile uchar* ptr_stencil,
    local volatile sub_group_mask_t* ptr_temp,
    uint c_blending_color, uint c_blending_data,
    uint c_depth_data,
    uint c_render_mode_flags,
    uint c_stencil_data
)
{
    blend_shader_input_t blend_shader_input;
    blend_shader_output_t blend_shader_output;

    uint color = float4_to_uint(&fs_out->gl_FragColor);

    // per-fragment enabled operations
    bool stencil_test_enabled = (c_render_mode_flags & RENDER_MODE_FLAG_ENABLE_STENCIL) != 0;
    bool depth_test_enabled = (c_render_mode_flags & RENDER_MODE_FLAG_ENABLE_DEPTH) != 0;

    // blend_shader_input.needs_dst = false;

    sub_group_mask_t sub_group_mask;
    clear_sub_group_mask(&sub_group_mask);
    set_bit_sub_group_mask(&sub_group_mask, get_sub_group_local_id());

    sub_group_mask_t lt_mask = get_lane_sub_group_mask_lt();
    // TODO: Optimize, ordering on triangle is not always required.
    do
    {
        clear_sub_group_mask(ptr_temp);
        atomic_or_sub_group_mask(ptr_temp, sub_group_mask);

        if (!any_sub_group_mask(and_sub_group_mask(*ptr_temp, lt_mask))) {
            
            // stencil test
            uchar stencil = *ptr_stencil;
            
            if (stencil_test_enabled) {
                if (!stencil_test(stencil, c_stencil_data)) {
                    stencil_operation(ptr_stencil, c_stencil_data, get_stencil_operation_sfail(c_stencil_data));
                    // *ptr_stencil = *ptr_stencil;
                    return;
                }
            }

            // depth test
            if (depth_test_enabled) {
                if (!depth_test(depth, *ptr_depth, c_depth_data)) {
                    if (stencil_test_enabled)
                        stencil_operation(ptr_stencil, c_stencil_data, get_stencil_operation_dfail(c_stencil_data));
                    return;
                }
                *ptr_depth = depth;
            }

            // stencil op post depth test
            if (stencil_test_enabled) {
                stencil_operation(ptr_stencil, c_stencil_data, get_stencil_operation_dpass(c_stencil_data));
                // *ptr_stencil = 1;
            }
            
            // blending
            run_blend_shader(&blend_shader_input, &blend_shader_output, tri_idx, pixel_x, pixel_y, 0, color, *ptr_color,
                c_blending_color, c_blending_data, c_render_mode_flags);
            if (blend_shader_output.write_color)
                *ptr_color = blend_shader_output.color;
            
            return;
        }
    }
    while (true);
    
}
#endif // DEVICE_SUB_GROUP_RAW_ENABLED

#endif // BACKEND_SHADERS_ROP_CL