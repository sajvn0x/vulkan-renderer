#version 450 core

layout(location = 0) in vec4 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec3 in_vertex_color;

layout(location = 0) out vec3 out_camera_pos_in_light;
layout(location = 1) out vec3 out_plight;
layout(location = 2) out vec3 out_nlight;
layout(location = 3) out vec4 out_shadow_uv;
layout(location = 4) out vec3 out_surface_color;

layout(set = 0, binding = 0) uniform TransformParams {
    mat4 model_view_proj;
    mat4 object_to_world;
    mat4 object_to_world_it;
    mat4 world_to_light;
    mat4 world_to_light_tt;
    mat4 world_to_shadow_proj;

    vec3 camera_pos_in_world;
    float shadow_bias;
}
Transform;

void main() {
    gl_Position = Transform.model_view_proj * in_position;
    vec4 p_world = Transform.object_to_world * in_position;
    vec3 n_world = (Transform.object_to_world_it * vec4(in_normal, 0.0)).xyz;
    out_camera_pos_in_light =
        (Transform.world_to_light * vec4(Transform.camera_pos_in_world, 1.0)).xyz;

    out_plight = (Transform.world_to_light * p_world).xyz;
    out_nlight = (Transform.world_to_light_tt * vec4(n_world, 0.0)).xyz;

    vec4 shadow_proj = Transform.world_to_shadow_proj * p_world;

    out_shadow_uv.xy = 0.5 * (shadow_proj.xy + shadow_proj.ww);
    out_shadow_uv.z = 0.5 * (shadow_proj.z + shadow_proj.w - Transform.shadow_bias);
    out_shadow_uv.w = shadow_proj.w;
    out_surface_color = in_vertex_color;
}
