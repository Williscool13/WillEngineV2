//
// Created by William on 2024-08-24.
//

#ifndef RENDER_OBJECT_H
#define RENDER_OBJECT_H

#include <string_view>
#include <vector>
#include <vulkan/vulkan_core.h>

#include "game_object.h"
#include "../renderer/vk_types.h"
#include "../renderer/vk_descriptor_buffer.h"

class Engine;

struct RenderNode
{
    Transform transform;
    std::vector<RenderNode> children;
    RenderNode* parent;
    int meshIndex{-1};
};

class RenderObject {
public:
    RenderObject() = default;
    RenderObject(Engine* engine, std::string_view gltfFilepath);
    ~RenderObject();

private:
    // samplers
    // images
    // pbr material data
    // vertices and indices
    // nodes



    //void BindDescriptors(VkCommandBuffer cmd);
    // indirects are then constructed/reconstructed every frame elsewhere in the code.
    // When ready to draw, the game will bind, the render object resources

    // the game will adjust the pipeline object as necessary (without shader objects this is slightly less flexible)
    // the game will then execute drawIndirectDrawBuffers

private:
    std::vector<Mesh> meshes;

    std::vector<RenderNode> renderNodes;
    std::vector<int32_t> topNodes;

    std::vector<VkDrawIndexedIndirectCommand> drawIndirectCommands;
    std::vector<InstanceData> instanceDatas;

public:
    GameObject* GenerateGameObject();

    void draw(const VkCommandBuffer& cmd);

private: // Drawing
    AllocatedBuffer vertexBuffer{};
    AllocatedBuffer indexBuffer{};
    AllocatedBuffer drawIndirectBuffer{};

    AllocatedBuffer materialBuffer{};

    DescriptorBufferSampler textureDescriptorBuffer;


    GameObject* RecursiveGenerateGameObject(const RenderNode& renderNode);

    void UploadIndirect();

private:
    Engine* creator;

    VkDescriptorSetLayout bufferAddressesDescriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorSetLayout textureDescriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorSetLayout computeCullingDescriptorSetLayout{VK_NULL_HANDLE};
};



#endif //RENDER_OBJECT_H
