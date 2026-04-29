#define ATTRIBUTE_VEC3 position, color

#define VARYING_VEC3 out_color

#define UNIFORM_MAT4 model, perspective, view

#define UNIFORM_FLOAT blend

#include <wrapper.cl>

VS_MAIN({
    gl_Position    = mul(perspective, mul(view, mul(model,(vec4){position, 1})));
    out_color      = color;
})

FS_MAIN({
    gl_FragColor = (vec4){out_color, blend};
})
