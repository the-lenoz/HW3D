#version 460 core

layout(local_size_x=64, local_size_y=1, local_size_z=1) in;

struct Triangle
{
    vec3 a;
    vec3 b;
    vec3 c;
};

layout(std430, binding=0) readonly buffer TriangleBuffer {
    Triangle triangles[];
};

layout(std430, binding=1) writeonly buffer HighlightBuffer {
    int highlighted[];
};

void main()
{
    uint index = gl_GlobalInvocationID.x;
    if (index >= triangles.length()) return;

    Triangle t = triangles[index];

    float S = length(cross(t.b-t.a, t.c-t.a)) / 2;

    highlighted[index] = S > 5.0 ? 1 : 0;
}
