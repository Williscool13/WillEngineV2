//
// Created by William on 2024-08-24.
//

#ifndef RENDER_OBJECT_H
#define RENDER_OBJECT_H

#include <string_view>
#include <vulkan/vulkan_core.h>

#include "engine.h"

struct RenderMesh
{
    uint32_t meshIndex;
    uint32_t vertexOffset;
    uint32_t indexStart;
    uint32_t indexCount;
};

class RenderObject {
public:
    RenderObject() = default;
    RenderObject(Engine* engine, std::string_view gltfFilepath);

private:
    // samplers
    // images
    // pbr material data
    // vertices and indices
    // nodes

    VkDescriptorSetLayout bufferAddressesDescriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorSetLayout textureDescriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorSetLayout computeCullingDescriptorSetLayout{VK_NULL_HANDLE};

    void BindDescriptors(VkCommandBuffer cmd);
    // indirects are then constructed/reconstructed every frame elsewhere in the code.
    // When ready to draw, the game will bind, the render object resources

    // the game will adjust the pipeline object as necessary (without shader objects this is slightly less flexible)
    // the game will then execute drawIndirectDrawBuffers

private:
    std::vector<RenderMesh> meshes;

    AllocatedBuffer vertexBuffer;
    AllocatedBuffer indexBuffer;
};



#endif //RENDER_OBJECT_H
