#version 330 core

in vec2 in_texCoord;
in vec3 position;

out vec2 out_texCoord;

void main()
{
    out_texCoord = in_texCoord;
    gl_Position = vec4(position, 1);
}
