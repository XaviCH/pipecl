#define ATTRIBUTE_VEC3 position, in_color

#define VARYING_VEC3 out_color

#ifdef __COMPILER_RELATIVE_PATH__
    #include <backend/shaders/wrapper.cl>
#else
    #include "glsc2/src/backend/shaders/wrapper.cl"
#endif

VS_MAIN({
    gl_Position = (vec4)(position, 1);
    out_color = in_color;
})

FS_MAIN({
    gl_FragColor = (vec4)(out_color, 1);
})