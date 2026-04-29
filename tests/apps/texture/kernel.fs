#version 330 core

in vec2 out_texCoord;

uniform sampler2D ourTexture;

void main()
{
    gl_FragColor = texture(ourTexture, out_texCoord);
}