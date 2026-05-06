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

    Error err = driver_context.initialize();
    ERR_FAIL_COND_V(err != OK, err);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);

    window =
        glfwCreateWindow(mode->width, mode->height, APP_NAME, nullptr, nullptr);
    ERR_FAIL_COND_V_MSG(window == nullptr, err, "Failed to create window");

    return OK;
}

Engine::Engine() {}

Engine::~Engine() {
    glfwDestroyWindow(window);
    glfwTerminate();
}
