#version 460

layout(local_size_x = 64) in;

const float PI = 3.14159265358979323846;
const float FULL_TURN = 2.0 * PI;

struct InitialTriangle {
    vec4 a;
    vec4 b;
    vec4 c;
    vec4 axis_origin;
    vec4 axis_direction_and_speed;
};

struct RuntimeTriangle {
    uvec4 a_and_highlight;
    vec4 b;
    vec4 c;
    vec4 min_bound;
    vec4 max_bound;
};

layout(std430, set = 0, binding = 0) readonly buffer InitialBuffer {
    InitialTriangle initial_triangles[];
};
layout(std430, set = 0, binding = 1) writeonly buffer RuntimeBuffer {
    RuntimeTriangle runtime_triangles[];
};

layout(push_constant) uniform RotationPushConstants {
    float elapsed_seconds;
} push_constants;

vec3 safely_normalized(vec3 value)
{
    float component_scale = max(abs(value.x), max(abs(value.y), abs(value.z)));
    vec3 scaled = value / component_scale;
    return scaled * inversesqrt(dot(scaled, scaled));
}

vec3 rotate_vertex(vec3 vertex, vec3 origin, vec3 axis, float cosine, float sine)
{
    vec3 radius = vertex - origin;
    return origin + radius * cosine + cross(axis, radius) * sine
        + axis * dot(axis, radius) * (1.0 - cosine);
}

void main()
{
    uint index = gl_GlobalInvocationID.x;
    if (index >= initial_triangles.length()) return;

    InitialTriangle source = initial_triangles[index];
    vec3 a = source.a.xyz;
    vec3 b = source.b.xyz;
    vec3 c = source.c.xyz;
    float speed = source.axis_direction_and_speed.w;
    float angle = speed * push_constants.elapsed_seconds;
    angle -= roundEven(angle / FULL_TURN) * FULL_TURN;

    if (speed != 0.0 && angle != 0.0) {
        vec3 origin = source.axis_origin.xyz;
        vec3 axis = safely_normalized(source.axis_direction_and_speed.xyz);
        float sine = sin(angle);
        float cosine = cos(angle);
        a = rotate_vertex(a, origin, axis, cosine, sine);
        b = rotate_vertex(b, origin, axis, cosine, sine);
        c = rotate_vertex(c, origin, axis, cosine, sine);
    }

    vec3 minimum = min(a, min(b, c));
    vec3 maximum = max(a, max(b, c));
    runtime_triangles[index].a_and_highlight = uvec4(floatBitsToUint(a), 0u);
    runtime_triangles[index].b = vec4(b, 0.0);
    runtime_triangles[index].c = vec4(c, 0.0);
    runtime_triangles[index].min_bound = vec4(minimum, 0.0);
    runtime_triangles[index].max_bound = vec4(maximum, 0.0);
}
