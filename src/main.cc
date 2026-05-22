#include <spirv_reflect.h>

#include <iostream>

#include "engine.hh"
#include "renderer/shader_container.hh"

int main(void) {
    // Engine engine;
    // if (engine.initialize() != OK) {
    //     return -1;
    // }
    //
    // engine.run();

    ShaderProgram shader_program;
    shader_program.load_shader_program("terrain");

    return 0;
}
