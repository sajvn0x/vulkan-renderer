#include "engine.hh"

#include "core/error/error_macros.hh"

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

    window = glfwCreateWindow(800, 600, "Vulkan", nullptr, nullptr);
    ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to create window");

    return OK;
}

Engine::Engine() {}

Engine::~Engine() {
    glfwDestroyWindow(window);
    glfwTerminate();
}
