#version 460

struct RuntimeTriangle {
    uvec4 a_and_highlight;
    vec4 b;
    vec4 c;
    vec4 min_bound;
    vec4 max_bound;
};

layout(std430, set = 0, binding = 0) readonly buffer RuntimeBuffer {
    RuntimeTriangle triangles[];
};
layout(push_constant) uniform RendererPushConstants {
    mat4 view_projection;
} push_constants;

layout(location = 0) noperspective out vec3 fragment_barycentric;
layout(location = 1) flat out uint fragment_highlighted;
layout(location = 2) flat out vec3 fragment_normal;

void main()
{
    uint triangle_index = uint(gl_VertexIndex) / 3u;
    uint vertex_index = uint(gl_VertexIndex) % 3u;
    RuntimeTriangle triangle = triangles[triangle_index];
    vec3 a = uintBitsToFloat(triangle.a_and_highlight.xyz);
    vec3 b = triangle.b.xyz;
    vec3 c = triangle.c.xyz;
    vec3 positions[3] = vec3[](a, b, c);
    vec3 barycentrics[3] = vec3[](
        vec3(1.0, 0.0, 0.0),
        vec3(0.0, 1.0, 0.0),
        vec3(0.0, 0.0, 1.0));

    vec3 normal = cross(b - a, c - a);
    float normal_length = length(normal);
    fragment_normal = normal_length == 0.0
        ? vec3(0.0, 1.0, 0.0) : normal / normal_length;
    fragment_barycentric = barycentrics[vertex_index];
    fragment_highlighted = triangle.a_and_highlight.w;
    gl_Position = push_constants.view_projection
        * vec4(positions[vertex_index], 1.0);
}
