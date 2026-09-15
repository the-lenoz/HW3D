#version 460 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 barycentric;
layout(location = 2) in float highlighted;

uniform mat4 view_projection;

noperspective out vec3 fragment_barycentric;
flat out float fragment_highlighted;

void main()
{
    fragment_barycentric = barycentric;
    fragment_highlighted = highlighted;
    gl_Position = view_projection * vec4(position, 1.0);
}
