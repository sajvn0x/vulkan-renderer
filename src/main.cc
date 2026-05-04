#include <GLFW/glfw3.h>

#include "renderer/driver_context.hh"

int main(void) {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    RenderingDriverContext driver_context = RenderingDriverContext();
    if (driver_context.initialize() != OK) {
        return -1;
    }

    GLFWwindow* window = glfwCreateWindow(800, 600, "Vulkan", nullptr, nullptr);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
