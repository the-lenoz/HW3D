#version 460 core

layout(location = 0) out vec4 fragment_color;

in noperspective vec3 fragment_barycentric;
flat in float fragment_highlighted;
flat in vec3 fragment_normal;

void main()
{
    const vec3 regular_color = vec3(0.55);
    const vec3 highlighted_color = vec3(0.85, 0.08, 0.08);
    const vec3 outline_color = vec3(0.0);
    const float ambient_strength = 0.15;
    const float diffuse_strength = 0.85;

    vec3 direction_to_light = normalize(vec3(-0.35, 1.0, 0.45));
    vec3 surface_normal = normalize(
        gl_FrontFacing ? fragment_normal : -fragment_normal);
    float diffuse_factor = max(
        dot(surface_normal, direction_to_light),
        0.0);
    float light_intensity = ambient_strength
        + diffuse_strength * diffuse_factor;

    vec3 base_color = mix(
        regular_color,
        highlighted_color,
        step(0.5, fragment_highlighted));
    vec3 fill_color = base_color * light_intensity;
    float distance_to_edge = min(
        fragment_barycentric.x,
        min(fragment_barycentric.y, fragment_barycentric.z));
    float outline_blend = smoothstep(
        0.0,
        fwidth(distance_to_edge) * 1.5,
        distance_to_edge);

    fragment_color = vec4(
        mix(outline_color, fill_color, outline_blend),
        1.0);
}
