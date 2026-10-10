#pragma once

#include <GLFW/glfw3.h>

#include "core/error/error_list.hh"
#include "renderer.hh"

class Engine {
private:
    float delta_time;

    GLFWwindow* window;
    Renderer* renderer = nullptr;

public:
    Error initialize();
    void run();

    Engine();
    ~Engine();
};
