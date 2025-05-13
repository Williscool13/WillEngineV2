//
// Created by William on 22/01/2025.
//

#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#include "engine/core/engine.h"

int main(int argc, char* argv[])
{
    will_engine::Engine engine;

    engine.init();

    engine.run();

    engine.cleanup();

    return 0;
}
