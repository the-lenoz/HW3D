#version 460 core

layout(local_size_x=64, local_size_y=1, local_size_z=1) in;

const int PROJECTION_XY = 0;
const int PROJECTION_XZ = 1;
const int PROJECTION_YZ = 2;

const int PRIMITIVE_POINT = 0;
const int PRIMITIVE_SEGMENT = 1;
const int PRIMITIVE_TRIANGLE = 2;

struct RuntimeTriangle
{
    uvec4 a_and_highlight;
    vec4 b;
    vec4 c;
    vec4 min_bound;
    vec4 max_bound;
};

struct Triangle
{
    vec3 a;
    vec3 b;
    vec3 c;
    vec3 min_bound;
    vec3 max_bound;
};

struct DoubleTriangle
{
    dvec3 a;
    dvec3 b;
    dvec3 c;
};

struct Primitive
{
    int type;
    dvec3 first;
    dvec3 second;
    DoubleTriangle triangle;
    int projection;
};

struct StaticEntry
{
    ivec4 voxel;
};

layout(std430, binding=0) buffer TriangleBuffer {
    RuntimeTriangle runtime_triangles[];
};

layout(std430, binding=2) readonly buffer PairBuffer {
    uvec2 pairs[];
};

layout(std430, binding=3) readonly buffer StaticEntryBuffer {
    StaticEntry static_entries[];
};

uniform int operation_mode;
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

    while (count > 0) {
        int step = count >> 1;
        int mid = left + step;
        bool is_smaller = isLess(static_entries[mid].voxel.xyz, target);

        if (is_smaller) {
            left = mid + 1;
            count -= step + 1;
        } else {
            count = step;
        }
    }

    bool found = left < arr_size
        && all(equal(static_entries[left].voxel.xyz, target));
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

bool samePoint(dvec3 left, dvec3 right)
{
    return all(equal(left, right));
}

dvec2 project(dvec3 point, int projection)
{
    if (projection == PROJECTION_XY) return point.xy;
    if (projection == PROJECTION_XZ) return point.xz;
    return point.yz;
}

double orient2d(dvec2 a, dvec2 b, dvec2 c)
{
    dvec2 ac = a - c;
    dvec2 bc = b - c;
    precise double left = ac.x * bc.y;
    precise double right = ac.y * bc.x;
    precise double determinant = left - right;
    return determinant;
}

double orient3d(dvec3 a, dvec3 b, dvec3 c, dvec3 d)
{
    dvec3 ad = a - d;
    dvec3 bd = b - d;
    dvec3 cd = c - d;

    precise double bdxcdy = bd.x * cd.y;
    precise double cdxbdy = cd.x * bd.y;
    precise double cdxady = cd.x * ad.y;
    precise double adxcdy = ad.x * cd.y;
    precise double adxbdy = ad.x * bd.y;
    precise double bdxady = bd.x * ad.y;
    precise double determinant = ad.z * (bdxcdy - cdxbdy)
        + bd.z * (cdxady - adxcdy)
        + cd.z * (adxbdy - bdxady);
    return determinant;
}

bool isZero(double value)
{
    return value == 0.0LF;
}

bool opposite(double left, double right)
{
    return (left < 0.0LF && right > 0.0LF)
        || (left > 0.0LF && right < 0.0LF);
}

bool between(double value, double first, double second)
{
    return value >= min(first, second) && value <= max(first, second);
}

bool pointInBox(dvec3 point, dvec3 first, dvec3 second)
{
    return between(point.x, first.x, second.x)
        && between(point.y, first.y, second.y)
        && between(point.z, first.z, second.z);
}

bool projectedCollinear(dvec3 a, dvec3 b, dvec3 c, int projection)
{
    return isZero(orient2d(
        project(a, projection),
        project(b, projection),
        project(c, projection)));
}

bool pointOnSegment(dvec3 point, dvec3 first, dvec3 second)
{
    return projectedCollinear(first, second, point, PROJECTION_XY)
        && projectedCollinear(first, second, point, PROJECTION_XZ)
        && projectedCollinear(first, second, point, PROJECTION_YZ)
        && pointInBox(point, first, second);
}

Primitive makePrimitive(DoubleTriangle value)
{
    Primitive result;
    result.first = value.a;
    result.second = value.a;
    result.triangle = value;
    result.projection = PROJECTION_XY;

    if (samePoint(value.a, value.b) && samePoint(value.a, value.c)) {
        result.type = PRIMITIVE_POINT;
        return result;
    }

    double xy = orient2d(value.a.xy, value.b.xy, value.c.xy);
    double xz = orient2d(value.a.xz, value.b.xz, value.c.xz);
    double yz = orient2d(value.a.yz, value.b.yz, value.c.yz);
    if (!isZero(xy) || !isZero(xz) || !isZero(yz)) {
        result.type = PRIMITIVE_TRIANGLE;
        result.projection = !isZero(xy)
            ? PROJECTION_XY
            : !isZero(xz) ? PROJECTION_XZ : PROJECTION_YZ;
        return result;
    }

    result.type = PRIMITIVE_SEGMENT;
    dvec3 points[3] = dvec3[](value.a, value.b, value.c);
    int coordinate = value.a.x != value.b.x || value.a.x != value.c.x
        ? 0
        : value.a.y != value.b.y || value.a.y != value.c.y ? 1 : 2;
    int minimum_index = 0;
    int maximum_index = 0;
    for (int index = 1; index < 3; ++index) {
        if (points[index][coordinate] < points[minimum_index][coordinate]) {
            minimum_index = index;
        }
        if (points[index][coordinate] > points[maximum_index][coordinate]) {
            maximum_index = index;
        }
    }
    result.first = points[minimum_index];
    result.second = points[maximum_index];
    return result;
}

bool pointOnSegment2d(dvec2 point, dvec2 first, dvec2 second)
{
    return between(point.x, first.x, second.x)
        && between(point.y, first.y, second.y);
}

bool segmentsIntersect2d(dvec2 a, dvec2 b, dvec2 c, dvec2 d)
{
    double abc = orient2d(a, b, c);
    double abd = orient2d(a, b, d);
    double cda = orient2d(c, d, a);
    double cdb = orient2d(c, d, b);
    if (opposite(abc, abd) && opposite(cda, cdb)) return true;

    return (isZero(abc) && pointOnSegment2d(c, a, b))
        || (isZero(abd) && pointOnSegment2d(d, a, b))
        || (isZero(cda) && pointOnSegment2d(a, c, d))
        || (isZero(cdb) && pointOnSegment2d(b, c, d));
}

bool pointInTriangle2d(
    dvec3 point,
    DoubleTriangle triangle,
    int projection)
{
    dvec2 p = project(point, projection);
    double first = orient2d(
        project(triangle.a, projection),
        project(triangle.b, projection),
        p);
    double second = orient2d(
        project(triangle.b, projection),
        project(triangle.c, projection),
        p);
    double third = orient2d(
        project(triangle.c, projection),
        project(triangle.a, projection),
        p);
    bool has_negative = first < 0.0LF || second < 0.0LF || third < 0.0LF;
    bool has_positive = first > 0.0LF || second > 0.0LF || third > 0.0LF;
    return !(has_negative && has_positive);
}

bool pointIntersectsTriangle(dvec3 point, Primitive triangle)
{
    return isZero(orient3d(
            triangle.triangle.a,
            triangle.triangle.b,
            triangle.triangle.c,
            point))
        && pointInTriangle2d(
            point,
            triangle.triangle,
            triangle.projection);
}

bool boxesOverlap(dvec3 a, dvec3 b, dvec3 c, dvec3 d)
{
    return max(min(a.x, b.x), min(c.x, d.x))
            <= min(max(a.x, b.x), max(c.x, d.x))
        && max(min(a.y, b.y), min(c.y, d.y))
            <= min(max(a.y, b.y), max(c.y, d.y))
        && max(min(a.z, b.z), min(c.z, d.z))
            <= min(max(a.z, b.z), max(c.z, d.z));
}

bool segmentsIntersect(dvec3 a, dvec3 b, dvec3 c, dvec3 d)
{
    if (!isZero(orient3d(a, b, c, d))) return false;

    for (int projection = PROJECTION_XY;
         projection <= PROJECTION_YZ;
         ++projection) {
        if (!projectedCollinear(a, b, c, projection)
            || !projectedCollinear(a, b, d, projection)) {
            return segmentsIntersect2d(
                project(a, projection),
                project(b, projection),
                project(c, projection),
                project(d, projection));
        }
    }
    return boxesOverlap(a, b, c, d);
}

bool signsAreConsistent(double first, double second, double third)
{
    bool has_negative = first < 0.0LF || second < 0.0LF || third < 0.0LF;
    bool has_positive = first > 0.0LF || second > 0.0LF || third > 0.0LF;
    return !(has_negative && has_positive);
}

bool coplanarSegmentIntersectsTriangle(
    dvec3 first,
    dvec3 second,
    Primitive triangle)
{
    if (pointInTriangle2d(
            first, triangle.triangle, triangle.projection)
        || pointInTriangle2d(
            second, triangle.triangle, triangle.projection)) {
        return true;
    }

    dvec3 vertices[3] = dvec3[](
        triangle.triangle.a,
        triangle.triangle.b,
        triangle.triangle.c);
    for (int index = 0; index < 3; ++index) {
        if (segmentsIntersect2d(
                project(first, triangle.projection),
                project(second, triangle.projection),
                project(vertices[index], triangle.projection),
                project(vertices[(index + 1) % 3], triangle.projection))) {
            return true;
        }
    }
    return false;
}

bool segmentIntersectsTriangle(
    dvec3 first,
    dvec3 second,
    Primitive triangle)
{
    double first_side = orient3d(
        triangle.triangle.a,
        triangle.triangle.b,
        triangle.triangle.c,
        first);
    double second_side = orient3d(
        triangle.triangle.a,
        triangle.triangle.b,
        triangle.triangle.c,
        second);
    if (isZero(first_side) && isZero(second_side)) {
        return coplanarSegmentIntersectsTriangle(first, second, triangle);
    }
    if (isZero(first_side)) return pointIntersectsTriangle(first, triangle);
    if (isZero(second_side)) return pointIntersectsTriangle(second, triangle);
    if (!opposite(first_side, second_side)) return false;

    return signsAreConsistent(
        orient3d(first, second, triangle.triangle.a, triangle.triangle.b),
        orient3d(first, second, triangle.triangle.b, triangle.triangle.c),
        orient3d(first, second, triangle.triangle.c, triangle.triangle.a));
}

bool triangleIntersectsTriangle(Primitive left, Primitive right)
{
    dvec3 left_vertices[3] = dvec3[](
        left.triangle.a, left.triangle.b, left.triangle.c);
    dvec3 right_vertices[3] = dvec3[](
        right.triangle.a, right.triangle.b, right.triangle.c);
    for (int index = 0; index < 3; ++index) {
        if (segmentIntersectsTriangle(
                left_vertices[index],
                left_vertices[(index + 1) % 3],
                right)) {
            return true;
        }
    }
    for (int index = 0; index < 3; ++index) {
        if (segmentIntersectsTriangle(
                right_vertices[index],
                right_vertices[(index + 1) % 3],
                left)) {
            return true;
        }
    }
    return false;
}

bool primitivesIntersect(Primitive left, Primitive right)
{
    if (left.type == PRIMITIVE_POINT) {
        if (right.type == PRIMITIVE_POINT) {
            return samePoint(left.first, right.first);
        }
        if (right.type == PRIMITIVE_SEGMENT) {
            return pointOnSegment(left.first, right.first, right.second);
        }
        return pointIntersectsTriangle(left.first, right);
    }
    if (left.type == PRIMITIVE_SEGMENT) {
        if (right.type == PRIMITIVE_POINT) {
            return pointOnSegment(right.first, left.first, left.second);
        }
        if (right.type == PRIMITIVE_SEGMENT) {
            return segmentsIntersect(
                left.first, left.second, right.first, right.second);
        }
        return segmentIntersectsTriangle(left.first, left.second, right);
    }
    if (right.type == PRIMITIVE_POINT) {
        return pointIntersectsTriangle(right.first, left);
    }
    if (right.type == PRIMITIVE_SEGMENT) {
        return segmentIntersectsTriangle(right.first, right.second, left);
    }
    return triangleIntersectsTriangle(left, right);
}

bool triangleIntersect(Triangle first, Triangle second)
{
    DoubleTriangle double_first = DoubleTriangle(
        dvec3(first.a),
        dvec3(first.b),
        dvec3(first.c));
    DoubleTriangle double_second = DoubleTriangle(
        dvec3(second.a),
        dvec3(second.b),
        dvec3(second.c));
    return primitivesIntersect(
        makePrimitive(double_first),
        makePrimitive(double_second));
}

Triangle loadTriangle(uint triangle_id)
{
    RuntimeTriangle packed_triangle = runtime_triangles[triangle_id];
    Triangle result;
    result.a = uintBitsToFloat(packed_triangle.a_and_highlight.xyz);
    result.b = packed_triangle.b.xyz;
    result.c = packed_triangle.c.xyz;
    result.min_bound = packed_triangle.min_bound.xyz;
    result.max_bound = packed_triangle.max_bound.xyz;
    return result;
}

void testPair(uint first_id, uint second_id)
{
    Triangle first = loadTriangle(first_id);
    Triangle second = loadTriangle(second_id);
    if (boundsIntersect(
            first.min_bound,
            first.max_bound,
            second.min_bound,
            second.max_bound)
        && triangleIntersect(first, second)) {
        atomicOr(runtime_triangles[first_id].a_and_highlight.w, 1u);
        atomicOr(runtime_triangles[second_id].a_and_highlight.w, 1u);
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
                int current_idx = findFirstBinarySearch(
                    check_voxel,
                    static_entries.length());

                while (current_idx != -1
                    && current_idx < static_entries.length())
                {
                    StaticEntry candidate = static_entries[current_idx];
                    if (!all(equal(candidate.voxel.xyz, check_voxel))) break;
                    if (current_idx > int(index)) {
                        testPair(triangle_id, uint(candidate.voxel.w));
                    }
                    ++current_idx;
                }
            }
        }
    }
}

void main()
{
    if (operation_mode == 0) {
        uint index = gl_GlobalInvocationID.x;
        if (index < uint(runtime_triangles.length())) {
            runtime_triangles[index].a_and_highlight.w = 0u;
        }
    } else if (operation_mode == 1) {
        runPairMode();
    } else {
        runVoxelFallback();
    }
}
