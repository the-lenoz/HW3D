#version 460 core
#extension GL_ARB_shader_group_vote : require

#define N_STEPS 22
#define SAT_AXIS_COUNT 17

layout(local_size_x=64, local_size_y=1, local_size_z=1) in;

struct Triangle
{
    vec3 a;
    vec3 b;
    vec3 c;
    vec3 min_bound;
    vec3 max_bound;
};

struct StaticEntry
{
    ivec4 voxel;
};

layout(std430, binding=0) readonly buffer TriangleBuffer {
    Triangle triangles[];
};

layout(std430, binding=1) buffer HighlightBuffer {
    int highlighted[];
};

layout(std430, binding=2) readonly buffer PairBuffer {
    uvec2 pairs[];
};

layout(std430, binding=3) readonly buffer StaticEntryBuffer {
    StaticEntry static_entries[];
};

uniform int pair_mode;
uniform uint pair_offset;

bool isLess(ivec3 a, ivec3 b)
{
    bvec3 lt = lessThan(a, b);
    bvec3 eq = equal(a, b);

    return lt.x || (eq.x && (lt.y || (eq.y && lt.z)));
}

int findFirstBinarySearch(ivec3 target, int arr_size)
{
    int left = 0;
    int count = arr_size;

    for (int i = 0; i < N_STEPS; i++) {
        int step = count >> 1;
        int mid = left + step;
        int safe_mid = min(mid, arr_size - 1);

        bool is_smaller = isLess(static_entries[safe_mid].voxel.xyz, target);

        left = is_smaller ? (safe_mid + 1) : left;
        count = is_smaller ? (count - step - 1) : step;
    }

    bool found = (left < arr_size)
        && (static_entries[left].voxel.xyz == target);
    return found ? left : -1;
}

bool boundsIntersect(
    vec3 min_bound_a,
    vec3 max_bound_a,
    vec3 min_bound_b,
    vec3 max_bound_b)
{
    return min_bound_a.x <= max_bound_b.x && min_bound_b.x <= max_bound_a.x &&
           min_bound_a.y <= max_bound_b.y && min_bound_b.y <= max_bound_a.y &&
           min_bound_a.z <= max_bound_b.z && min_bound_b.z <= max_bound_a.z;
}

bool projectedIntersect(Triangle a, Triangle b, vec3 axis)
{
    vec3 p1 = vec3(dot(a.a, axis), dot(a.b, axis), dot(a.c, axis));
    vec3 p2 = vec3(dot(b.a, axis), dot(b.b, axis), dot(b.c, axis));

    float min1 = min(p1.x, min(p1.y, p1.z));
    float max1 = max(p1.x, max(p1.y, p1.z));

    float min2 = min(p2.x, min(p2.y, p2.z));
    float max2 = max(p2.x, max(p2.y, p2.z));

    return min1 <= max2 && min2 <= max1;
}

bool triangleIntersect(Triangle a, Triangle b)
{
    vec3 v1 = a.b - a.a;
    vec3 u1 = a.c - a.a;
    vec3 w1 = a.c - a.b;

    vec3 v2 = b.b - b.a;
    vec3 u2 = b.c - b.a;
    vec3 w2 = b.c - b.b;

    vec3 n1 = cross(v1, u1);
    vec3 n2 = cross(v2, u2);

    vec3 axis[SAT_AXIS_COUNT] = vec3[](
        cross(v1, u1),
        cross(v2, u2),

        cross(v1, v2),
        cross(v1, u2),
        cross(v1, w2),

        cross(u1, v2),
        cross(u1, u2),
        cross(u1, w2),

        cross(w1, v2),
        cross(w1, u2),
        cross(w1, w2),

        cross(n1, v1),
        cross(n1, u1),
        cross(n1, w1),

        cross(n2, v2),
        cross(n2, u2),
        cross(n2, w2)
    );

    bool result = dot(n1, n1) > 0.0 && dot(n2, n2) > 0.0;
    for (int i = 0; i < SAT_AXIS_COUNT; ++i)
    {
        result = result && projectedIntersect(a, b, axis[i]);
    }
    return result;
}

void testPair(uint first_id, uint second_id)
{
    Triangle first = triangles[first_id];
    Triangle second = triangles[second_id];
    if (boundsIntersect(
            first.min_bound,
            first.max_bound,
            second.min_bound,
            second.max_bound)
        && triangleIntersect(first, second)) {
        atomicOr(highlighted[first_id], 1);
        atomicOr(highlighted[second_id], 1);
    }
}

void runPairMode()
{
    uint pair_index = pair_offset + gl_GlobalInvocationID.x;
    if (pair_index >= uint(pairs.length())) return;

    uvec2 candidate = pairs[pair_index];
    testPair(candidate.x, candidate.y);
}

void runVoxelFallback()
{
    uint index = gl_GlobalInvocationID.x;
    if (index >= uint(static_entries.length())) return;

    StaticEntry entry = static_entries[index];
    uint triangle_id = uint(entry.voxel.w);

    for (int dX = -1; dX <= 1; ++dX)
    {
        for (int dY = -1; dY <= 1; ++dY)
        {
            for (int dZ = -1; dZ <= 1; ++dZ)
            {
                ivec3 check_voxel = entry.voxel.xyz + ivec3(dX, dY, dZ);
                int found_entry = findFirstBinarySearch(
                    check_voxel,
                    static_entries.length());

                bool keep_scanning = found_entry != -1;
                int current_idx = found_entry;
                while (anyInvocationARB(keep_scanning))
                {
                    if (keep_scanning)
                    {
                        StaticEntry candidate = static_entries[current_idx];
                        if (candidate.voxel.xyz == check_voxel)
                        {
                            if (current_idx > int(index))
                            {
                                testPair(
                                    triangle_id,
                                    uint(candidate.voxel.w));
                            }
                        }
                        else
                        {
                            keep_scanning = false;
                        }

                        if (current_idx >= static_entries.length() - 1)
                        {
                            keep_scanning = false;
                        }
                        current_idx++;
                    }
                }
            }
        }
    }
}

void main()
{
    if (pair_mode != 0) {
        runPairMode();
    } else {
        runVoxelFallback();
    }
}
