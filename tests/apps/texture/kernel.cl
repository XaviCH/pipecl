#define ATTRIBUTE_VEC2 in_texCoord
#define ATTRIBUTE_VEC3 position

#define VARYING_VEC2 out_texCoord

#define UNIFORM_SAMPLER2D ourTexture

#include <wrapper.cl>

VS_MAIN({
    gl_Position = (vec4)(position, 1);
    out_texCoord = in_texCoord;
})

FS_MAIN({
    gl_FragColor = TEXTURE2D(ourTexture, out_texCoord);
})
