//
// Created by William on 2025-03-15.
//

#ifndef TEXTURE_H
#define TEXTURE_H
#include <filesystem>

#include "src/renderer/vk_types.h"


namespace will_engine
{
class Texture
{
public:
    Texture(std::filesystem::path source);

    ~Texture();

private:
    AllocatedImage texture;

private:
    std::filesystem::path source;
    std::string name;
};
}


#endif //TEXTURE_H
