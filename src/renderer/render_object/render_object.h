//
// Created by William on 2025-01-24.
//

#ifndef MODEL_H
#define MODEL_H

#include <filesystem>

#include "render_object_types.h"
#include "render_reference.h"
#include "extern/fastgltf/include/fastgltf/types.hpp"
#include "src/renderer/resource_manager.h"


namespace will_engine
{
class GameObject;

class RenderObject : public IRenderReference
{
public:
    RenderObject(const std::filesystem::path& gltfFilepath, ResourceManager& resourceManager);

    ~RenderObject() override;

    bool canDraw() const { return instanceBufferSize > 0; }
    const DescriptorBufferUniform& getAddressesDescriptorBuffer() const { return addressesDescriptorBuffer; }
    const DescriptorBufferSampler& getTextureDescriptorBuffer() const { return textureDescriptorBuffer; }
    [[nodiscard]] const AllocatedBuffer& getVertexBuffer() const { return vertexBuffer; }
    [[nodiscard]] const AllocatedBuffer& getIndexBuffer() const { return indexBuffer; }
    [[nodiscard]] const AllocatedBuffer& getIndirectBuffer() const { return drawIndirectBuffer; }
    [[nodiscard]] size_t getDrawIndirectCommandCount() const { return drawCommands.size(); }

    bool attachToGameObject(GameObject* gameObject, int32_t meshIndex);

    void updateInstanceData(int32_t instanceIndex, const glm::mat4& currentFrameModelMatrix) override;

private: // Model Data
    bool parseGltf(const std::filesystem::path& gltfFilepath);

    std::optional<AllocatedImage> loadImage(const fastgltf::Asset& asset, const fastgltf::Image& image, const std::filesystem::path& parentFolder) const;

    static Material extractMaterial(fastgltf::Asset& gltf, const fastgltf::Material& gltfMaterial);

    static void loadTextureIndices(const fastgltf::Optional<fastgltf::TextureInfo>& texture, const fastgltf::Asset& gltf, int& imageIndex, int& samplerIndex);

private: // Buffer Data
    bool generateBuffers();

    void expandInstanceBuffer(uint32_t countToAdd, bool copyPrevious = true);

    void uploadCullingBufferData();

    [[nodiscard]] InstanceData* getInstanceData(int32_t index) const;

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
     * The number of instances in the instance buffer
     */
    uint32_t instanceBufferSize{0};

    std::vector<VkDrawIndexedIndirectCommand> drawCommands{};
    std::vector<uint32_t> boundingSphereIndices;

    AllocatedBuffer vertexBuffer{};
    AllocatedBuffer indexBuffer{};
    AllocatedBuffer drawIndirectBuffer{};

    // addresses
    AllocatedBuffer addressBuffer{};
    //  the actual buffers
    AllocatedBuffer materialBuffer{};
    AllocatedBuffer modelMatrixBuffer{};

    DescriptorBufferUniform addressesDescriptorBuffer;
    DescriptorBufferSampler textureDescriptorBuffer;

    AllocatedBuffer meshBoundsBuffer{};
    AllocatedBuffer cullingAddressBuffer{};
    AllocatedBuffer boundingSphereIndicesBuffer{};
    DescriptorBufferUniform frustumCullingDescriptorBuffer;
};
}

#endif //MODEL_H
