#version 460 core

struct Triangle
{
    uvec4 a_and_highlight;
    vec4 b;
    vec4 c;
    vec4 min_bound;
    vec4 max_bound;
};

layout(std430, binding = 0) readonly buffer TriangleBuffer
{
    Triangle triangles[];
};

uniform mat4 view_projection;

noperspective out vec3 fragment_barycentric;
flat out uint fragment_highlighted;
flat out vec3 fragment_normal;

void main()
{
    uint triangle_index = uint(gl_VertexID) / 3u;
    uint vertex_index = uint(gl_VertexID) % 3u;
    Triangle triangle = triangles[triangle_index];
    vec3 a = uintBitsToFloat(triangle.a_and_highlight.xyz);
    vec3 b = triangle.b.xyz;
    vec3 c = triangle.c.xyz;

    vec3 position;
    vec3 barycentric;
    if (vertex_index == 0u) {
        position = a;
        barycentric = vec3(1.0, 0.0, 0.0);
    } else if (vertex_index == 1u) {
        position = b;
        barycentric = vec3(0.0, 1.0, 0.0);
    } else {
        position = c;
        barycentric = vec3(0.0, 0.0, 1.0);
    }

    fragment_barycentric = barycentric;
    vec3 normal = cross(b - a, c - a);
    float normal_length = length(normal);
    if (isnan(normal_length)
        || isinf(normal_length)
        || normal_length <= 1.1920928955078125e-7) {
        normal = vec3(0.0, 1.0, 0.0);
    } else {
        normal /= normal_length;
    }

    fragment_highlighted = triangle.a_and_highlight.w;
    fragment_normal = normal;
    gl_Position = view_projection * vec4(position, 1.0);
}
