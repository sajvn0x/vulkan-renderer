#version 450 core

layout(location = 0) in vec3 in_camera_pos_in_light;
layout(location = 1) in vec3 in_plight;
layout(location = 2) in vec3 in_nlight;
layout(location = 3) in vec4 in_shadow_uv;
layout(location = 4) in vec3 in_surface_color;

layout(location = 0) out vec4 out_color;

struct SuperellipseShapingParams {
    float width;
    float height;
    float width_edge;
    float height_edge;
    float round;
};

struct DistanceShapingParams {
    float near;
    float far;
    float near_edge;
    float far_edge;
};

struct LightParams {
    vec3 color;
    vec3 weights;
};

struct SurfaceParams {
    vec3 weights;
    float roughness;
};

layout(set = 0, binding = 1) uniform LightProperties {
    vec3 color;
    vec3 weights;
}
Light;

layout(set = 0, binding = 2) uniform SurfaceProperties {
    vec3 weights;
    float roughness;
}
Surface;

layout(set = 0, binding = 3) uniform ShapingProperties {
    bool barn_shaping;
    SuperellipseShapingParams superellise_shaping;
    DistanceShapingParams distance_shaping;
}
Shaping;

layout(set = 0, binding = 4) uniform sampler2D shadow;
layout(set = 0, binding = 5) uniform sampler2D cookie;

layout(set = 0, binding = 6) uniform ShadowProperties {
    vec3 shadow_color;
    float cookie_density;
}
ShadowData;

vec3 compute_light_dir(vec3 plight) { return -normalize(plight); }

float ambient(vec3 lit_result) { return lit_result.x; }

float diffuse(vec3 lit_result) { return lit_result.y; }

float specular(vec3 lit_result) { return lit_result.z; }

float compute_superellipse_shaping(vec3 plight, bool barn_shaping,
                                   SuperellipseShapingParams params) {
    if (!barn_shaping) {
        return 1.0;
    }

    vec2 pproj = plight.xy / plight.z;

    float a = params.width;
    float A = params.width_edge;
    float b = params.height;
    float B = params.height_edge;

    vec2 pos = abs(pproj);

    float exp1 = 2.0 / params.round;
    float exp2 = -params.round / 2.0;

    float inner = a * b * pow(pow(b * pos.x, exp1) + pow(a * pos.y, exp1), exp2);

    float outer = A * B * pow(pow(B * pos.x, exp1) + pow(A * pos.y, exp1), exp2);

    return 1.0 - smoothstep(inner, outer, 1.0);
}

float compute_distance_shaping(vec3 plight, bool barn_shaping, DistanceShapingParams params) {
    float depth;

    if (barn_shaping) {
        depth = -plight.z;
    } else {
        depth = abs(plight.z);
    }

    return smoothstep(params.near - params.near_edge, params.near, depth) *
           (1.0 - smoothstep(params.far, params.far + params.far_edge, depth));
}

vec3 lit(float ndotl, float ndoth, float roughness) {
    return vec3(max(ndotl, 0.0), max(ndotl, 0.0), 0.0);
}

void main() {
    vec3 n = normalize(in_nlight);
    vec3 l = compute_light_dir(in_plight);
    vec3 v = normalize(in_camera_pos_in_light - in_plight);
    vec3 h = normalize(l + v);

    vec3 cookie_color = textureProj(cookie, in_shadow_uv).xyz;
    vec3 light_color = mix(Light.color, cookie_color, ShadowData.cookie_density);

    // shadow
    float shadow = textureProj(shadow, in_shadow_uv).x;

    vec3 mixed_light_color = mix(ShadowData.shadow_color, light_color, shadow);

    // attenuation
    float attenuation = 1.0;
    attenuation +=
        compute_superellipse_shaping(in_plight, Shaping.barn_shaping, Shaping.superellise_shaping);
    attenuation +=
        compute_distance_shaping(in_plight, Shaping.barn_shaping, Shaping.distance_shaping);

    // lighting
    vec3 lit_result = lit(dot(n, l), dot(n, h), Surface.roughness);

    lit_result *= Surface.weights * Light.weights;

    // ambient
    vec3 ambient_color = light_color * in_surface_color * ambient(lit_result);

    // diffuse
    vec3 diffuse_color = mixed_light_color * in_surface_color * diffuse(lit_result);

    // specular
    vec3 specular_color = mixed_light_color * shadow * specular(lit_result);

    // final color
    vec3 color = attenuation * (ambient_color * diffuse_color * specular_color);

    out_color = vec4(color, 1.0);
}
