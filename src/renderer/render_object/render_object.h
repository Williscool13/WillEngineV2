//
// Created by William on 2025-01-24.
//

#ifndef MODEL_H
#define MODEL_H

#include <filesystem>

#include "render_object_types.h"
#include "render_reference.h"
#include "src/core/game_object/renderable.h"
#include "src/renderer/renderer_constants.h"
#include "src/renderer/resource_manager.h"
#include "src/renderer/pipelines/basic_compute/basic_compute_pipeline.h"


namespace will_engine
{
class GameObject;


struct RenderableProperties
{
    int32_t instanceIndex;
};

class RenderObject final : public IRenderReference
{
public:
    RenderObject(const std::filesystem::path& gltfFilepath, ResourceManager& resourceManager, uint32_t renderObjectId);

    ~RenderObject() override;

    void update(int32_t currentFrameOverlap, int32_t previousFrameOverlap);

    bool updateBuffers(int32_t currentFrameOverlap, int32_t previousFrameOverlap);

    void dirty() { bufferFramesToUpdate = FRAME_OVERLAP; }

    int32_t bufferFramesToUpdate{0};

private:
    int32_t getFreeInstanceIndex();

    std::unordered_set<uint32_t> freeInstanceIndices{};
    int32_t currentInstanceCount{0};


    /**
     * IRenderable : updateCount map.
     */
    std::unordered_map<IRenderable*, RenderableProperties> renderableMap;

public: // IRenderReference
    [[nodiscard]] uint32_t getId() const override { return renderObjectId; }

    bool releaseInstanceIndex(IRenderable* renderable) override;


private: // IRenderReference
    /**
     * Hash of the file path
     */
    uint32_t renderObjectId{};

public: // Model Rendering API
    GameObject* generateGameObject(const std::string& gameObjectName = "");

    [[nodiscard]] size_t getMeshCount() const { return meshes.size(); }
    bool canDraw() const { return currentInstanceCount != freeInstanceIndices.size(); }
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

private: // Model Parsing
    bool parseGltf(const std::filesystem::path& gltfFilepath);

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
