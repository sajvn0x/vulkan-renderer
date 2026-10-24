#version 450 core

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec2 in_uv;

layout (location = 0) out vec3 out_normal;
layout (location = 1) out vec2 out_uv;

void main() {
	gl_Position = vec4(in_pos, 1.0);
	out_uv = in_uv;
	out_normal = in_normal;
}

