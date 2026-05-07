#pragma once

#include <GLFW/glfw3.h>

#include "core/error/error_list.hh"
#include "renderer/rendering_device.hh"

class Engine {
   private:
    float delta_time;

    // window
    GLFWwindow *window;

    // renderer
    RenderingDevice rendering_device;

   public:
    Error initialize();
    void run();

    Engine();
    ~Engine();
};
