#version 450 core

layout(location = 0) in vec4 in_vertex;

layout(location = 0) out vec2 out_tex_coord;

layout(set = 0, binding = 0) uniform Projection { mat4 projection; }
u_projection;

void main() {
    gl_Position = u_projection.projection * vec4(in_vertex.xy, 0.0, 0.1);
    out_tex_coord = in_vertex.zw;
}
