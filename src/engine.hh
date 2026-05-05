#pragma once

#include <GLFW/glfw3.h>

#include "core/error/error_list.hh"
#include "renderer/driver_context.hh"

class Engine {
   private:
    float delta_time;

    // window
    GLFWwindow* window;

    // renderer
    RenderingDriverContext driver_context;

   public:
    Error initialize();
    void run();

    Engine();
    ~Engine();
};
