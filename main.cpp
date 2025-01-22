//
// Created by William on 22/01/2025.
//

#define VMA_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "src/core/engine.h"

int main(int argc, char* argv[])
{
    Engine engine;

    engine.init();

    engine.run();

    engine.cleanup();

    return 0;
}
