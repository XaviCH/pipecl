#version 330 core

in vec3 position;
in vec3 in_color;

uniform mat4 perspective;
uniform mat4 view;
uniform mat4 model;

out vec3 out_color;

void main()
{
    gl_Position = perspective * view * model * vec4(position, 1);
    out_color = in_color;
}
