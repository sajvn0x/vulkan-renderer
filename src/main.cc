#include <volk.h>

#include <iostream>

int main(void) {
    if (volkInitialize() != VK_SUCCESS) {
        std::cout << "failed to load the vulkan" << std::endl;
    }

    return 0;
}
