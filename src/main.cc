#include "engine.hh"

int main(void) {
    Engine engine;
    if (engine.initialize() != OK) {
        return -1;
    }

    engine.run();

    return 0;
}
