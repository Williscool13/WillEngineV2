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

    [[nodiscard]] bool canDraw() const { return vertexBuffer.buffer != VK_NULL_HANDLE; }

    const DescriptorBufferUniform& getAddressesDescriptorBuffer() { return addressesDescriptorBuffer; }
    const DescriptorBufferSampler& getTextureDescriptorBuffer() { return textureDescriptorBuffer; }
    [[nodiscard]] const AllocatedBuffer& getVertexBuffer() const { return vertexBuffer; }
    [[nodiscard]] const AllocatedBuffer& getIndexBuffer() const { return indexBuffer; }
    [[nodiscard]] const AllocatedBuffer& getIndirectBuffer() const { return drawIndirectBuffer; }
    [[nodiscard]] size_t getDrawIndirectCommandCount() const { return drawIndirectCommands.size(); }

private: // Drawing
    AllocatedBuffer vertexBuffer{};
    AllocatedBuffer indexBuffer{};
    AllocatedBuffer drawIndirectBuffer{};

    // addresses
    AllocatedBuffer bufferAddresses;
    //  the actual buffers
    AllocatedBuffer materialBuffer{};
    AllocatedBuffer instanceBuffer{};


    DescriptorBufferUniform addressesDescriptorBuffer;
    DescriptorBufferSampler textureDescriptorBuffer;


    void RecursiveGenerateGameObject(const RenderNode& renderNode, GameObject* parent);

    void UploadIndirect();

private:
    Engine* creator{nullptr};

    static int renderObjectCount;
    static constexpr size_t MAX_SAMPLER_COUNT{8};
    static constexpr size_t MAX_IMAGES_COUNT{64};
public:
    static VkDescriptorSetLayout addressesDescriptorSetLayout;
    static VkDescriptorSetLayout textureDescriptorSetLayout;
    static VkDescriptorSetLayout computeCullingDescriptorSetLayout;

};



#endif //RENDER_OBJECT_H
