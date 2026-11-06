#version 450 core

layout(location = 0) in vec2 in_tex_coord;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 1) uniform sampler2D text;
layout(set = 0, binding = 2) uniform TextColor { vec3 value; }
u_text_color;

void main() {
    float alpha = texture(text, in_tex_coord).r;
    out_color = vec4(u_text_color.value, 1.0) * vec4(1.0, 1.0, 1.0, alpha);
}
