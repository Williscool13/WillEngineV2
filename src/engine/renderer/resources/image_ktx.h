//
// Created by William on 2025-05-27.
//

#ifndef IMAGE_KTX_H
#define IMAGE_KTX_H
#include "image_resource.h"
#include "ktxvulkan.h"

namespace will_engine::renderer
{

struct ImageKtx final : ImageResource {

protected:
    ktxVulkanTexture texture;
};

}

#endif //IMAGE_KTX_H
