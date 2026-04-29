#define ATTRIBUTE_VEC3 position, color

#define VARYING_VEC3 out_color

#include <wrapper.cl>

VS_MAIN({
    gl_Position    = (vec4){position, 1};
    out_color  = color;
})

FS_MAIN({
    gl_FragColor = (vec4){out_color, 1};
})
