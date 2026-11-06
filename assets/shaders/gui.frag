#version 450 core

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform texture2D tex;
layout(set = 1, binding = 0) uniform sampler samp;

layout(location = 0) in struct {
    vec4 color;
    vec2 uv;
} In;

void main() { out_color = In.color * texture(sampler2D(tex, samp), In.uv.st); }
