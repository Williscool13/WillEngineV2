//
// Created by William on 2025-01-24.
//

#ifndef MODEL_H
#define MODEL_H

#include <filesystem>
#include <optional>
#include <fastgltf/types.hpp>

#include "render_object_types.h"
#include "render_reference.h"
#include "src/renderer/renderer_constants.h"
#include "src/renderer/resource_manager.h"
#include "src/renderer/pipelines/basic_compute/basic_compute_pipeline.h"
#include "src/util/math_constants.h"


namespace will_engine
{
class GameObject;

class RenderObject final : public IRenderReference
{
public:
    RenderObject(const std::filesystem::path& gltfFilepath, ResourceManager& resourceManager, int64_t renderObjectId);

    ~RenderObject() override;

    void update(int32_t currentFrameOverlap, int32_t previousFrameOverlap);

    void dirty() { framesToUpdate = FRAME_OVERLAP; }

    int32_t framesToUpdate{FRAME_OVERLAP};

private:
    int32_t getFreeInstanceIndex();

    std::unordered_set<uint32_t> freeInstanceIndices{};
    int32_t currentInstanceCount{0};

public: // IRenderReference
    void setId(const uint64_t identifier) override { renderObjectId = identifier; }

    [[nodiscard]] uint64_t getId() const override { return renderObjectId; }

private: // IIdentifiable
    /**
     * Hash of the file path
     */
    uint32_t renderObjectId{};
    std::filesystem::path gltfFilepath;

public:
    GameObject* generateGameObject(const std::string& gameObjectName = "");

    int32_t getMeshCount() const { return meshes.size(); }
    bool canDraw() const { return currentInstanceCount > 0; }
    const DescriptorBufferUniform& getAddressesDescriptorBuffer() const { return addressesDescriptorBuffer; }
    const DescriptorBufferSampler& getTextureDescriptorBuffer() const { return textureDescriptorBuffer; }
    const DescriptorBufferUniform& getFrustumCullingAddressesDescriptorBuffer() { return frustumCullingDescriptorBuffer; }
    [[nodiscard]] const AllocatedBuffer& getVertexBuffer() const { return vertexBuffer; }
    [[nodiscard]] const AllocatedBuffer& getIndexBuffer() const { return indexBuffer; }
    [[nodiscard]] const AllocatedBuffer& getIndirectBuffer(const int32_t currentFrameOverlap) const { return drawIndirectBuffers[currentFrameOverlap]; }
    [[nodiscard]] size_t getDrawIndirectCommandCount() const { return drawCommands.size(); }

    void recursiveGenerateGameObject(const RenderNode& renderNode, GameObject* parent);

    /**
     * @param gameObject to assign mesh references.
     * @param meshIndex in the RenderObject to generate. \code 0 < n <= meshes.size()\endcode
     * @return true if successfully generated mesh and assigned to gameobject.
     */
    bool generateMesh(GameObject* gameObject, int32_t meshIndex);

    [[nodiscard]] uint64_t getRenderReferenceIndex() const override { return renderObjectId; }

    [[nodiscard]] const std::filesystem::path& getFilePath() const { return gltfFilepath; }

    void updateInstanceData(int32_t instanceIndex, const CurrentInstanceData& newInstanceData, int32_t currentFrameOverlap, int32_t previousFrameOverlap) override;

    bool releaseInstanceIndex(uint32_t instanceIndex) override;

private: // Model Data
    bool parseGltf(const std::filesystem::path& gltfFilepath);

    std::optional<AllocatedImage> loadImage(const fastgltf::Asset& asset, const fastgltf::Image& image, const std::filesystem::path& parentFolder) const;

    static Material extractMaterial(fastgltf::Asset& gltf, const fastgltf::Material& gltfMaterial);

    static void loadTextureIndices(const fastgltf::Optional<fastgltf::TextureInfo>& texture, const fastgltf::Asset& gltf, int& imageIndex, int& samplerIndex);

    static VkFilter extractFilter(fastgltf::Filter filter);

    static VkSamplerMipmapMode extractMipMapMode(fastgltf::Filter filter);

private: // Buffer Data
    bool generateBuffers();

    bool releaseBuffers();

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
    std::vector<VkDrawIndexedIndirectCommand> drawCommands{};
    std::vector<uint32_t> boundingSphereIndices;

    AllocatedBuffer vertexBuffer{};
    AllocatedBuffer indexBuffer{};
    AllocatedBuffer drawIndirectBuffers[FRAME_OVERLAP]{};

    // addresses
    AllocatedBuffer addressBuffers[FRAME_OVERLAP]{};
    //  the actual buffers
    AllocatedBuffer materialBuffer{};
    AllocatedBuffer modelMatrixBuffers[FRAME_OVERLAP]{};

    DescriptorBufferUniform addressesDescriptorBuffer;
    DescriptorBufferSampler textureDescriptorBuffer;

    AllocatedBuffer meshBoundsBuffer{};
    AllocatedBuffer cullingAddressBuffers[FRAME_OVERLAP]{};
    AllocatedBuffer boundingSphereIndicesBuffers[FRAME_OVERLAP]{};
    DescriptorBufferUniform frustumCullingDescriptorBuffer;
};
}

#endif //MODEL_H
