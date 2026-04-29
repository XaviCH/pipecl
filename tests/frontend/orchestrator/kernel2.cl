#define ATTRIBUTE_VEC2 in_coord
#define ATTRIBUTE_VEC3 position

#define VARYING_VEC2 out_coord

#define UNIFORM_SAMPLER2D sampl

#include <wrapper.cl>

VS_MAIN({
    gl_Position = (vec4)(position, 1);
    out_coord = in_coord;
})

FS_MAIN({
    gl_FragColor = TEXTURE2D(sampl, out_coord);
})
