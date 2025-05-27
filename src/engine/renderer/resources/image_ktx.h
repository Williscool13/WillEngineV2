//
// Created by William on 2025-05-27.
//

#ifndef IMAGE_KTX_H
#define IMAGE_KTX_H
#include "image_resource.h"

namespace will_engine::renderer
{

class ImageKtx : public ImageResource {

    ktxVulkanTexture texture;
};

}

#endif //IMAGE_KTX_H
