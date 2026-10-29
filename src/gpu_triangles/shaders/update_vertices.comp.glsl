#version 460 core

layout(local_size_x=64, local_size_y=1, local_size_z=1) in;

const double PI = 3.1415926535897932384626433832795LF;
const double FULL_TURN = 2.0LF * PI;

struct InitialTriangle
{
    vec4 a;
    vec4 b;
    vec4 c;
    vec4 axis_origin;
    vec4 axis_direction_and_speed;
};

struct RuntimeTriangle
{
    uvec4 a_and_highlight;
    vec4 b;
    vec4 c;
    vec4 min_bound;
    vec4 max_bound;
};

layout(std430, binding=0) readonly buffer InitialTriangleBuffer
{
    InitialTriangle initial_triangles[];
};

layout(std430, binding=1) writeonly buffer RuntimeTriangleBuffer
{
    RuntimeTriangle runtime_triangles[];
};

uniform double elapsed_seconds;

dvec3 rotateVertex(
    dvec3 vertex,
    dvec3 origin,
    dvec3 axis,
    double cosine,
    double sine)
{
    dvec3 radius = vertex - origin;
    return origin
        + radius * cosine
        + cross(axis, radius) * sine
        + axis * dot(axis, radius) * (1.0LF - cosine);
}

void main()
{
    uint index = gl_GlobalInvocationID.x;
    if (index >= uint(initial_triangles.length())) return;

    InitialTriangle source = initial_triangles[index];
    vec3 a = source.a.xyz;
    vec3 b = source.b.xyz;
    vec3 c = source.c.xyz;
    float angular_velocity = source.axis_direction_and_speed.w;

    if (angular_velocity != 0.0) {
        double angle = double(angular_velocity) * elapsed_seconds;
        angle -= roundEven(angle / FULL_TURN) * FULL_TURN;
        if (angle != 0.0LF) {
            dvec3 origin = dvec3(source.axis_origin.xyz);
            dvec3 axis = normalize(dvec3(
                source.axis_direction_and_speed.xyz));
            float reduced_angle = float(angle);
            double sine = double(sin(reduced_angle));
            double cosine = double(cos(reduced_angle));
            a = vec3(rotateVertex(
                dvec3(source.a.xyz), origin, axis, cosine, sine));
            b = vec3(rotateVertex(
                dvec3(source.b.xyz), origin, axis, cosine, sine));
            c = vec3(rotateVertex(
                dvec3(source.c.xyz), origin, axis, cosine, sine));
        }
    }

    vec3 minimum = min(a, min(b, c));
    vec3 maximum = max(a, max(b, c));
    runtime_triangles[index].a_and_highlight = uvec4(
        floatBitsToUint(a), 0u);
    runtime_triangles[index].b = vec4(b, 0.0);
    runtime_triangles[index].c = vec4(c, 0.0);
    runtime_triangles[index].min_bound = vec4(minimum, 0.0);
    runtime_triangles[index].max_bound = vec4(maximum, 0.0);
}
