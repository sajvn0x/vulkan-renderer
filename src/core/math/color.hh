#pragma once

struct Color {
    union {
        struct {
            float r;
            float g;
            float b;
            float a;
        };
        float components[4] = {0, 0, 0, 1.0};
    };
};
