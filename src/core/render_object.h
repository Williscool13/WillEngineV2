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
#include "../renderer/acceleration-structures/bounding_sphere.h"

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
class RenderObject
{
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
    std::vector<BoundingSphere> boundingSpheres;

    std::vector<RenderNode> renderNodes;
    std::vector<int32_t> topNodes;

    std::vector<DrawIndirectData> drawIndirectData;
    uint32_t instanceBufferSize{0};

    /**
     * The number of mesh instances in the whole model
     */
    uint32_t instanceCount{0};

    /**
     * The number of mesh instances that have been instantiated
     */
    uint32_t activeInstanceCount{0};

public:
   GameObject* generateGameObject();

    [[nodiscard]] InstanceData* getInstanceData(int32_t index) const;

    [[nodiscard]] bool canDraw() const { return instanceBufferSize > 0; }

    const DescriptorBufferUniform& getAddressesDescriptorBuffer() { return addressesDescriptorBuffer; }
    const DescriptorBufferSampler& getTextureDescriptorBuffer() { return textureDescriptorBuffer; }
    [[nodiscard]] const AllocatedBuffer& getVertexBuffer() const { return vertexBuffer; }
    [[nodiscard]] const AllocatedBuffer& getIndexBuffer() const { return indexBuffer; }
    [[nodiscard]] const AllocatedBuffer& getIndirectBuffer() const { return drawIndirectBuffer; }
    [[nodiscard]] size_t getDrawIndirectCommandCount() const { return drawIndirectData.size(); }

    const DescriptorBufferUniform& getFrustumCullingAddressesDescriptorBuffer() { return frustumCullingDescriptorBuffer; }

private: // Drawing
    AllocatedBuffer vertexBuffer{};
    AllocatedBuffer indexBuffer{};
    AllocatedBuffer drawIndirectBuffer{};

    // addresses
    AllocatedBuffer bufferAddresses;
    //  the actual buffers
    AllocatedBuffer materialBuffer{};
    AllocatedBuffer modelMatrixBuffer{};

    // Culling
    AllocatedBuffer meshBoundsBuffer{};

    DescriptorBufferUniform addressesDescriptorBuffer;
    DescriptorBufferSampler textureDescriptorBuffer;

    AllocatedBuffer frustumBufferAddresses;
    AllocatedBuffer meshIndicesBuffer{};
    DescriptorBufferUniform frustumCullingDescriptorBuffer;

    void recursiveGenerateGameObject(const RenderNode& renderNode, GameObject* parent);

    /**
     * Expands the instance buffer of the render object by \code count\endcode amount.
     * \n Should be called whenever new instances are created
     * @param countToAdd
     * @param copy if true, will attempt to copy previous buffer into the new buffer
     */
    void expandInstanceBuffer(uint32_t countToAdd, bool copy = true);

    /**
     * Uploads the indirect buffer of the render object
     * \n Should be called whenever the indirect buffer is expanded
     */
    void uploadIndirectBuffer();

    /**
     * Updates the compute culling buffer with updated buffer addresses of the instance and indirect buffers
     * \n Should be called after updating either the instance/indirect buffer
     */
    void updateComputeCullingBuffer();

private:
    Engine* creator{nullptr};

    static int renderObjectCount;
    static constexpr size_t MAX_SAMPLER_COUNT{8};
    static constexpr size_t MAX_IMAGES_COUNT{80};

public:
    static VkDescriptorSetLayout addressesDescriptorSetLayout;
    static VkDescriptorSetLayout textureDescriptorSetLayout;
    static VkDescriptorSetLayout frustumCullingDescriptorSetLayout;
};


#endif //RENDER_OBJECT_H
