#define ATTRIBUTE_VEC3 position
#define ATTRIBUTE_VEC2 texCoord

#define VARYING_VEC2 out_texCoord

#define UNIFORM_MAT4 model, perspective, view

#define UNIFORM_SAMPLER2D ourTexture

#include <wrapper.cl>

VS_MAIN({
    gl_Position    = mul(perspective, mul(view, mul(model,(vec4){position, 1})));
    out_texCoord  = texCoord;
})

FS_MAIN({
    gl_FragColor = TEXTURE2D(ourTexture, out_texCoord);
})

#include <backend/pipeline/fine_raster.cl>