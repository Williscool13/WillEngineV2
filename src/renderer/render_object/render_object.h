//
// Created by William on 2025-01-24.
//

#ifndef MODEL_H
#define MODEL_H

#include <filesystem>

#include "render_object_types.h"
#include "render_reference.h"
#include "extern/fastgltf/include/fastgltf/types.hpp"
#include "src/renderer/renderer_constants.h"
#include "src/renderer/resource_manager.h"


namespace will_engine
{
class GameObject;

class RenderObject final : public IRenderReference
{
public:
    RenderObject(const std::filesystem::path& gltfFilepath, ResourceManager& resourceManager);

    ~RenderObject() override;

    GameObject* generateGameObject();

    bool canDraw() const { return instanceBufferCapacity > 0; }
    const DescriptorBufferUniform& getAddressesDescriptorBuffer() const { return addressesDescriptorBuffer; }
    const DescriptorBufferSampler& getTextureDescriptorBuffer() const { return textureDescriptorBuffer; }
    [[nodiscard]] const AllocatedBuffer& getVertexBuffer() const { return vertexBuffer; }
    [[nodiscard]] const AllocatedBuffer& getIndexBuffer() const { return indexBuffer; }
    [[nodiscard]] const AllocatedBuffer& getIndirectBuffer() const { return drawIndirectBuffer; }
    [[nodiscard]] size_t getDrawIndirectCommandCount() const { return drawCommands.size(); }

    GameObject* generateGameObject(int32_t meshIndex, const Transform& startingTransform);

    void recursiveGenerateGameObject(const RenderNode& renderNode, GameObject* parent);

    bool attachToGameObject(GameObject* gameObject, int32_t meshIndex);

    void updateInstanceData(int32_t instanceIndex, const glm::mat4& newModelMatrix) override;

private: // Model Data
    bool parseGltf(const std::filesystem::path& gltfFilepath);

    std::optional<AllocatedImage> loadImage(const fastgltf::Asset& asset, const fastgltf::Image& image, const std::filesystem::path& parentFolder) const;

    static Material extractMaterial(fastgltf::Asset& gltf, const fastgltf::Material& gltfMaterial);

    static void loadTextureIndices(const fastgltf::Optional<fastgltf::TextureInfo>& texture, const fastgltf::Asset& gltf, int& imageIndex, int& samplerIndex);

private: // Buffer Data
    bool generateBuffers();

    /**
     * Expand all model matrix buffers and copy contents of previous model matrix buffers into the new one.
     * \n Also update the addresses buffer to point to the new model matrix buffers
     * @param countToAdd
     * @param copyPrevious
     */
    void expandInstanceBuffer(uint32_t countToAdd, bool copyPrevious = true);

    void uploadCullingBufferData();

private: // Model Data
    ResourceManager& resourceManager;

    std::vector<VkSampler> samplers{};
    std::vector<AllocatedImage> images{};
    std::vector<Material> materials{};
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Mesh> meshes{};
    std::vector<BoundingSphere> boundingSpheres{};
    std::vector<RenderNode> renderNodes{};
    std::vector<int32_t> topNodes;

    static constexpr int32_t samplerOffset{1};
    static constexpr int32_t imageOffset{1};

private: // Buffer Data
    /**
     * The current number of active instance in the instance buffer
     */
    uint32_t instanceBufferSize{0};
    /**
     * The number of instances the instance buffer can contain
     */
    uint32_t instanceBufferCapacity{0};

    std::vector<VkDrawIndexedIndirectCommand> drawCommands{};
    std::vector<uint32_t> boundingSphereIndices;

    AllocatedBuffer vertexBuffer{};
    AllocatedBuffer indexBuffer{};
    AllocatedBuffer drawIndirectBuffer{};

    // addresses
    AllocatedBuffer addressBuffers[FRAME_OVERLAP]{};
    //  the actual buffers
    AllocatedBuffer materialBuffer{};
    AllocatedBuffer modelMatrixBuffers[FRAME_OVERLAP]{};

    DescriptorBufferUniform addressesDescriptorBuffer;
    DescriptorBufferSampler textureDescriptorBuffer;

    AllocatedBuffer meshBoundsBuffer{};
    AllocatedBuffer cullingAddressBuffers[FRAME_OVERLAP]{};
    AllocatedBuffer boundingSphereIndicesBuffer{};
    DescriptorBufferUniform frustumCullingDescriptorBuffer;
};
}

#endif //MODEL_H
