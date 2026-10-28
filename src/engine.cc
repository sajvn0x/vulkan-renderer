#include "engine.hh"

#include "core/error/error_macros.hh"
#include "version.hh"

Engine* Engine::singleton = nullptr;

Engine::Engine() { singleton = this; }

Engine::~Engine() {
    glfwDestroyWindow(window);
    glfwTerminate();

    renderer->~Renderer();
    singleton = nullptr;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        Engine::get_singleton()->is_app_running = false;
    }
}

void Engine::run() {
    while (is_app_running) {
        glfwPollEvents();
        glfwSetKeyCallback(window, key_callback);
        renderer->begin_frame();
        renderer->next_frame();
    }
}

Error Engine::initialize() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);

    window = glfwCreateWindow(mode->width, mode->height, APP_NAME, nullptr, nullptr);
    ERR_FAIL_COND_V_MSG(window == nullptr, ERR_CANT_CREATE, "Failed to create window");

    renderer = new Renderer();
    Error err = renderer->initialize(window);
    ERR_FAIL_COND_V_MSG(err != OK, ERR_CANT_CREATE, "Failed to create rendering device");

    return OK;
}
