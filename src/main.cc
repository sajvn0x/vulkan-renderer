#include "renderer/driver_context.hh"

int main(void) {
    RenderingDriverContext driver_context = RenderingDriverContext();

    if (driver_context.initialize() != OK) {
        return -1;
    }

    return 0;
}
