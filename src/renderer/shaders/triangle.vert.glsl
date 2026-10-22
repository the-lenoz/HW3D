#version 460 core

struct Triangle
{
    vec4 a;
    vec4 b;
    vec4 c;
    vec4 normal;
};

layout(std430, binding = 0) readonly buffer TriangleBuffer
{
    Triangle triangles[];
};

uniform mat4 view_projection;

noperspective out vec3 fragment_barycentric;
flat out float fragment_highlighted;
flat out vec3 fragment_normal;

void main()
{
    uint triangle_index = uint(gl_VertexID) / 3u;
    uint vertex_index = uint(gl_VertexID) % 3u;
    Triangle triangle = triangles[triangle_index];

    vec3 position;
    vec3 barycentric;
    if (vertex_index == 0u) {
        position = triangle.a.xyz;
        barycentric = vec3(1.0, 0.0, 0.0);
    } else if (vertex_index == 1u) {
        position = triangle.b.xyz;
        barycentric = vec3(0.0, 1.0, 0.0);
    } else {
        position = triangle.c.xyz;
        barycentric = vec3(0.0, 0.0, 1.0);
    }

    fragment_barycentric = barycentric;
    fragment_highlighted = triangle.a.w;
    fragment_normal = triangle.normal.xyz;
    gl_Position = view_projection * vec4(position, 1.0);
}
