#include "engine.hh"

#include "core/error/error_macros.hh"
#include "version.hh"

void Engine::run() {
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }
}

Error Engine::initialize() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);

    window = glfwCreateWindow(mode->width, mode->height, APP_NAME, nullptr, nullptr);
    ERR_FAIL_COND_V_MSG(window == nullptr, ERR_CANT_CREATE, "Failed to create window");

    Error err = rendering_device.initialize();
    ERR_FAIL_COND_V_MSG(err != OK, ERR_CANT_CREATE, "Failed to create rendering device");

    return OK;
}

Engine::Engine() {}

Engine::~Engine() {
    glfwDestroyWindow(window);
    glfwTerminate();
}
