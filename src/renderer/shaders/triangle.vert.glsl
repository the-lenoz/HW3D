#version 460 core

layout(location = 0) in vec3 position;

uniform mat4 view_projection;

void main()
{
    gl_Position = view_projection * vec4(position, 1.0);
}

