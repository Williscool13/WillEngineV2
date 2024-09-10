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
class RenderObject;

struct RenderNode
{
    Transform transform;
    std::vector<RenderNode> children;
    RenderNode* parent;
    int meshIndex{-1};
};

struct RenderObjectReference
{
    RenderObject* renderObject;
    int32_t instanceIndex;
};

/**
 * Lifetime: Program
 * Exists after initialized at program start.
 * Only deleted when application is exiting.
 */
class RenderObject {
public:
    RenderObject() = default;
    RenderObject(Engine* engine, std::string_view gltfFilepath);
    ~RenderObject();

private:
    void parseModel(Engine* engine, std::string_view gltfFilepath);

private:
    std::vector<VkSampler> samplers;
    std::vector<AllocatedImage> images;
    std::vector<Mesh> meshes;

    std::vector<RenderNode> renderNodes;
    std::vector<int32_t> topNodes;

    std::vector<VkDrawIndexedIndirectCommand> drawIndirectCommands;
    std::vector<InstanceData> instanceDatas;

public:
    GameObject* GenerateGameObject();

    void updateInstanceData(const InstanceData& value, int32_t index) const;

    void draw(const VkCommandBuffer cmd, VkPipelineLayout pipelineLayout);

private: // Drawing
    AllocatedBuffer vertexBuffer{};
    AllocatedBuffer indexBuffer{};
    AllocatedBuffer drawIndirectBuffer{};

    // addresses
    AllocatedBuffer bufferAddresses;
    //
    AllocatedBuffer materialBuffer{};
    AllocatedBuffer instanceBuffer{};


    DescriptorBufferUniform addressesDescriptorBuffer;
    DescriptorBufferSampler textureDescriptorBuffer;


    void RecursiveGenerateGameObject(const RenderNode& renderNode, GameObject* parent);

    void UploadIndirect();

private:
    Engine* creator;

    static int renderObjectCount;
    static constexpr size_t samplerCount{32}; // see hard coded values in the shader
    static constexpr size_t imageCount{255}; // if anything exists after images, need to use padding.
public:
    static VkDescriptorSetLayout addressesDescriptorSetLayout;
    static VkDescriptorSetLayout textureDescriptorSetLayout;
    static VkDescriptorSetLayout computeCullingDescriptorSetLayout;

};



#endif //RENDER_OBJECT_H
