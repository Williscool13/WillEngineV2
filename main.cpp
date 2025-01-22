//
// Created by William on 22/01/2025.
//

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include "src/core/engine.h"

int main(int argc, char* argv[])
{
    Engine engine;

    engine.init();

    engine.run();

    engine.cleanup();

    return 0;
}
