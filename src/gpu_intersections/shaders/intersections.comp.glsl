#version 460

layout(local_size_x = 64) in;

struct RuntimeTriangle {
    uvec4 a_and_highlight;
    vec4 b;
    vec4 c;
    vec4 min_bound;
    vec4 max_bound;
};

layout(std430, set = 0, binding = 0) buffer RuntimeBuffer {
    RuntimeTriangle runtime_triangles[];
};
layout(std430, set = 0, binding = 1) readonly buffer PairBuffer {
    uvec2 pairs[];
};
layout(std430, set = 0, binding = 2) readonly buffer StaticEntryBuffer {
    ivec4 static_entries[];
};

layout(push_constant) uniform IntersectionPushConstants {
    uint operation_mode;
    uint invocation_offset;
} push_constants;

struct Triangle {
    vec3 a;
    vec3 b;
    vec3 c;
    vec3 minimum;
    vec3 maximum;
};

Triangle load_triangle(uint id)
{
    RuntimeTriangle packed = runtime_triangles[id];
    return Triangle(
        uintBitsToFloat(packed.a_and_highlight.xyz),
        packed.b.xyz,
        packed.c.xyz,
        packed.min_bound.xyz,
        packed.max_bound.xyz);
}

bool bounds_intersect(Triangle a, Triangle b)
{
    return a.minimum.x <= b.maximum.x && b.minimum.x <= a.maximum.x
        && a.minimum.y <= b.maximum.y && b.minimum.y <= a.maximum.y
        && a.minimum.z <= b.maximum.z && b.minimum.z <= a.maximum.z;
}

float maximum_component(vec3 value)
{
    value = abs(value);
    return max(value.x, max(value.y, value.z));
}

bool separated_on_axis(
    vec3 axis,
    vec3 a0, vec3 a1, vec3 a2,
    vec3 b0, vec3 b1, vec3 b2)
{
    float ap0 = dot(a0, axis);
    float ap1 = dot(a1, axis);
    float ap2 = dot(a2, axis);
    float bp0 = dot(b0, axis);
    float bp1 = dot(b1, axis);
    float bp2 = dot(b2, axis);
    float minimum_a = min(ap0, min(ap1, ap2));
    float maximum_a = max(ap0, max(ap1, ap2));
    float minimum_b = min(bp0, min(bp1, bp2));
    float maximum_b = max(bp0, max(bp1, bp2));
    return maximum_a < minimum_b || maximum_b < minimum_a;
}

bool sat_intersects(Triangle left, Triangle right)
{
    vec3 origin = left.a;
    vec3 a0 = left.a - origin;
    vec3 a1 = left.b - origin;
    vec3 a2 = left.c - origin;
    vec3 b0 = right.a - origin;
    vec3 b1 = right.b - origin;
    vec3 b2 = right.c - origin;

    float pair_scale = max(maximum_component(a0), max(
        maximum_component(a1), max(maximum_component(a2), max(
        maximum_component(b0), max(maximum_component(b1),
        maximum_component(b2))))));
    float pair_divisor = pair_scale == 0.0 ? 1.0 : pair_scale;
    a0 /= pair_divisor;
    a1 /= pair_divisor;
    a2 /= pair_divisor;
    b0 /= pair_divisor;
    b1 /= pair_divisor;
    b2 /= pair_divisor;

    vec3 edges_a[3] = vec3[](a1 - a0, a2 - a1, a0 - a2);
    vec3 edges_b[3] = vec3[](b1 - b0, b2 - b1, b0 - b2);
    float edge_scale_a = max(maximum_component(edges_a[0]),
        max(maximum_component(edges_a[1]), maximum_component(edges_a[2])));
    float edge_scale_b = max(maximum_component(edges_b[0]),
        max(maximum_component(edges_b[1]), maximum_component(edges_b[2])));
    float edge_divisor_a = edge_scale_a == 0.0 ? 1.0 : edge_scale_a;
    float edge_divisor_b = edge_scale_b == 0.0 ? 1.0 : edge_scale_b;
    for (int i = 0; i < 3; ++i) {
        edges_a[i] /= edge_divisor_a;
        edges_b[i] /= edge_divisor_b;
    }

    vec3 normal_a = cross(edges_a[0], edges_a[1]);
    vec3 normal_b = cross(edges_b[0], edges_b[1]);
    bool degenerate_a = all(equal(normal_a, vec3(0.0)));
    bool degenerate_b = all(equal(normal_b, vec3(0.0)));

    uint separated = 0u;
    separated |= uint(separated_on_axis(
        normal_a, a0, a1, a2, b0, b1, b2));
    separated |= uint(separated_on_axis(
        normal_b, a0, a1, a2, b0, b1, b2));
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            separated |= uint(separated_on_axis(
                cross(edges_a[i], edges_b[j]),
                a0, a1, a2, b0, b1, b2));
        }
    }
    for (int i = 0; i < 3; ++i) {
        separated |= uint(separated_on_axis(
            cross(normal_a, edges_a[i]),
            a0, a1, a2, b0, b1, b2));
    }
    for (int i = 0; i < 3; ++i) {
        separated |= uint(separated_on_axis(
            cross(normal_b, edges_b[i]),
            a0, a1, a2, b0, b1, b2));
    }
    return !degenerate_a && !degenerate_b && separated == 0u;
}

void test_pair(uint left_id, uint right_id)
{
    Triangle left = load_triangle(left_id);
    Triangle right = load_triangle(right_id);
    if (bounds_intersect(left, right) && sat_intersects(left, right)) {
        atomicOr(runtime_triangles[left_id].a_and_highlight.w, 1u);
        atomicOr(runtime_triangles[right_id].a_and_highlight.w, 1u);
    }
}

bool voxel_less(ivec3 left, ivec3 right)
{
    return left.x < right.x || (left.x == right.x
        && (left.y < right.y || (left.y == right.y && left.z < right.z)));
}

int find_first(ivec3 target)
{
    int first = 0;
    int count = static_entries.length();
    while (count > 0) {
        int step = count / 2;
        int middle = first + step;
        if (voxel_less(static_entries[middle].xyz, target)) {
            first = middle + 1;
            count -= step + 1;
        } else {
            count = step;
        }
    }
    return first < static_entries.length()
        && all(equal(static_entries[first].xyz, target)) ? first : -1;
}

void run_voxel(uint index)
{
    if (index >= static_entries.length()) return;
    ivec4 entry = static_entries[index];
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            for (int z = -1; z <= 1; ++z) {
                ivec3 target = entry.xyz + ivec3(x, y, z);
                int candidate = find_first(target);
                while (candidate >= 0 && candidate < static_entries.length()
                    && all(equal(static_entries[candidate].xyz, target))) {
                    if (candidate > int(index)) {
                        test_pair(uint(entry.w), uint(static_entries[candidate].w));
                    }
                    ++candidate;
                }
            }
        }
    }
}

void main()
{
    uint invocation = push_constants.invocation_offset
        + gl_GlobalInvocationID.x;
    if (push_constants.operation_mode == 0u) {
        if (invocation < pairs.length()) {
            uvec2 pair = pairs[invocation];
            test_pair(pair.x, pair.y);
        }
    } else {
        run_voxel(invocation);
    }
}
