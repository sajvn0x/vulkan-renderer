#pragma once

#include <GLFW/glfw3.h>

#include "core/error/error_list.hh"
#include "renderer.hh"

class Engine {
    static Engine* singleton;

public:
    Engine();
    ~Engine();

    static Engine* get_singleton() { return singleton; }

    float delta_time;
    bool is_app_running = true;
    GLFWwindow* window;
    Renderer* renderer = nullptr;

    Error initialize();
    void run();
};
