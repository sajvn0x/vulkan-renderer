#version 450 core

layout (set = 0, binding = 0, std140) uniform SceneData {
    vec2 resolution;
    float time;
} scene_data;

const vec3 BG_COLOR = vec3(0.01, 0.16, 0.42);
const vec3 RECT_COLOR = vec3(0.01, 0.26, 0.57);

// Noise background
const float NOISE_INTENSITY = 2.8;
const float NOISE_DEFINITION = 0.6;
const vec2 GLOW_POS = vec2(-2.0, 0.0);

// Rectangles
const float TOTAL = 60.0;
const float MIN_SIZE = 0.03;
const float MAX_SIZE = 0.08 - MIN_SIZE;
const float Y_DISTRIBUTION = 0.5;


float random(vec2 co) {
    return fract(
        sin(dot(co, vec2(12.9898, 78.233)))
        * 43758.5453
    );
}


float noise(vec2 p) {
    p *= NOISE_INTENSITY;

    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);

    return mix(
        mix(
            random(i + vec2(0.0, 0.0)),
            random(i + vec2(1.0, 0.0)),
            u.x
        ),
        mix(
            random(i + vec2(0.0, 1.0)),
            random(i + vec2(1.0, 1.0)),
            u.x
        ),
        u.y
    );
}


float fbm(vec2 uv) {
    uv *= 5.0;

    mat2 m = mat2(
         1.6,  1.2,
        -1.2,  1.6
    );

    float f = 0.5000 * noise(uv);
    uv = m * uv;

    f += 0.2500 * noise(uv);
    uv = m * uv;

    f += 0.1250 * noise(uv);
    uv = m * uv;

    f += 0.0625 * noise(uv);

    f = 0.5 + 0.5 * f;

    return f;
}


vec3 bg(vec2 uv) {
    float velocity = scene_data.time / 1.6;

    float intensity =
        sin(uv.x * 3.0 + velocity * 2.0) * 1.1 + 1.5;

    uv.y -= 2.0;

    vec2 bp = uv + GLOW_POS;

    uv *= NOISE_DEFINITION;

    // Ripple
    float rb = fbm(
        vec2(uv.x * 0.5 - velocity * 0.03, uv.y)
    ) * 0.1;

    uv += rb;

    // Coloring
    float rz = fbm(
        uv * 0.9 + vec2(velocity * 0.35, 0.0)
    );

    rz *= dot(bp * intensity, bp) + 1.2;

    vec3 col = BG_COLOR / (0.1 - rz);

    return sqrt(abs(col));
}


float rectangle(
    vec2 uv,
    vec2 pos,
    float width,
    float height,
    float blur
) {
    pos = (vec2(width, height) + 0.01) / 2.0
        - abs(uv - pos);

    pos = smoothstep(vec2(0.0), vec2(blur), pos);

    return pos.x * pos.y;
}


mat2 rotate_2d(float angle) {
    return mat2(
        cos(angle), -sin(angle),
        sin(angle),  cos(angle)
    );
}


layout (location = 0) out vec4 out_color;

void main() {
    // Convert framebuffer coordinates to Shadertoy-style UVs.
    // Flip Y because Vulkan's default fragment-coordinate
    // origin is at the top-left.
    vec2 frag_coord = vec2(
        gl_FragCoord.x,
        scene_data.resolution.y - gl_FragCoord.y
    );

    vec2 uv = frag_coord / scene_data.resolution * 2.0 - 1.0;

    uv.x *= scene_data.resolution.x / scene_data.resolution.y;

    // Background
    vec3 color = bg(uv) * (2.0 - abs(uv.y * 2.0));

    // Animated rectangles
    float velX = -scene_data.time / 8.0;
    float velY =  scene_data.time / 10.0;

    for (float i = 0.0; i < TOTAL; i += 1.0) {
        float index = i / TOTAL;
        float rnd = random(vec2(index));

        vec3 pos = vec3(0.0);

        pos.x = fract(velX * rnd + index) * 4.0 - 2.0;
        pos.y = sin(index * rnd * 1000.0 + velY)
              * Y_DISTRIBUTION;
        pos.z = MAX_SIZE * rnd + MIN_SIZE;

        vec2 uv_rot = uv - pos.xy + pos.z / 2.0;

        uv_rot = rotate_2d(i + scene_data.time / 2.0) * uv_rot;

        uv_rot += pos.xy + pos.z / 2.0;

        float rect = rectangle(
            uv_rot,
            pos.xy,
            pos.z,
            pos.z,
            (MAX_SIZE + MIN_SIZE - pos.z) / 2.0
        );

        color += RECT_COLOR * rect * pos.z / MAX_SIZE;
    }

    out_color = vec4(color, 1.0);
}

