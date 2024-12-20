//
// Created by William on 2024-08-24.
//

#ifndef RENDER_OBJECT_H
#define RENDER_OBJECT_H

#include <vector>
#include <string_view>

#include <vulkan/vulkan_core.h>

#include "src/renderer/vk_descriptor_buffer.h"
#include "src/renderer/vk_types.h"
#include "src/util/transform.h"

class VulkanContext;
class ResourceManager;
struct InstanceData;
struct DrawIndirectData;
struct Mesh;
struct AllocatedImage;
struct BoundingSphere;

class Engine;
class GameObject;
class RenderObject;

struct RenderNode
{
    Transform transform;
    std::vector<RenderNode> children;
    RenderNode* parent;
    int32_t meshIndex{-1};
};

struct RenderObjectReference
{
    RenderObject* renderObject;
    /**
     * The instance index in the instance buffer of the render object
     */
    int32_t instanceIndex;
};

struct RenderObjectLayouts
{
    VkDescriptorSetLayout frustumCullLayout;
    VkDescriptorSetLayout addressesLayout;
    VkDescriptorSetLayout texturesLayout;
};

/**
 * Lifetime: Program
 * Exists after initialized at program start.
 * Only deleted when application is exiting.
 */
class RenderObject
{
public:
    RenderObject() = delete;

    RenderObject(const VulkanContext& context, const ResourceManager& resourceManager, std::string_view gltfFilepath, const RenderObjectLayouts& descriptorLayouts);

    ~RenderObject();

private:
    void parseModel(std::string_view gltfFilepath, VkDescriptorSetLayout texturesLayout, std::vector<BoundingSphere>& boundingSpheres);

public:
    /**
    * Generates a single game object with every single mesh in this render object, grouped under a single GameObject
    * \n Hierarchy matches the hierarchy of the gltf object
    * @return
    */
    GameObject* generateGameObject();

    /**
     * Generates a gameobject of the specified mesh
     * @param meshIndex
     * @param startingTransform
     * @return
     */
    GameObject* generateGameObject(int32_t meshIndex, const Transform& startingTransform = {});

    [[nodiscard]] InstanceData* getInstanceData(int32_t index) const;

    [[nodiscard]] const std::vector<Mesh>& getMeshes() const { return meshes; }

    [[nodiscard]] bool canDraw() const { return instanceBufferSize > 0 && currentInstanceCount > 0; }

    const DescriptorBufferUniform& getAddressesDescriptorBuffer() { return addressesDescriptorBuffer; }
    const DescriptorBufferSampler& getTextureDescriptorBuffer() { return textureDescriptorBuffer; }
    [[nodiscard]] const AllocatedBuffer& getVertexBuffer() const { return vertexBuffer; }
    [[nodiscard]] const AllocatedBuffer& getIndexBuffer() const { return indexBuffer; }
    [[nodiscard]] const AllocatedBuffer& getIndirectBuffer() const { return drawIndirectBuffer; }
    [[nodiscard]] size_t getDrawIndirectCommandCount() const { return drawIndirectData.size(); }

    const DescriptorBufferUniform& getFrustumCullingAddressesDescriptorBuffer() { return frustumCullingDescriptorBuffer; }

private:
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

private: // Model Data
    std::vector<VkSampler> samplers;
    std::vector<AllocatedImage> images;
    std::vector<Mesh> meshes;

    std::vector<RenderNode> renderNodes;
    std::vector<int32_t> topNodes;

    std::vector<DrawIndirectData> drawIndirectData;

    /**
     * The number of instances in the instance buffer
     */
    uint32_t instanceBufferSize{0};

    /**
     * The number of mesh instances that have been instantiated
     */
    uint32_t currentInstanceCount{0};

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

private:
    const VulkanContext& context;
    const ResourceManager& resourceManager;
};


#endif //RENDER_OBJECT_H
