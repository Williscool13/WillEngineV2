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


namespace will_engine
{
class GameObject;


struct RenderableProperties
{
    uint32_t instanceIndex;
};

/**
 * Render Objects are persistent class representations of GLTF files. They always exist and their lifetime is managed through \code AssetManager\endcode.
 * \n The Render Object can be loaded/unloaded at runtime to avoid unnecessary GPU allocations.
 */
class RenderObject final : public IRenderReference
{
public:
    RenderObject(ResourceManager& resourceManager, const std::filesystem::path& willmodelPath, const std::filesystem::path& gltfFilepath,
                 std::string name, uint32_t renderObjectId);

    ~RenderObject() override;

    void update(VkCommandBuffer cmd, int32_t currentFrameOverlap, int32_t previousFrameOverlap);

    /**
     * Buffers only update if change to instance count/primitive count. Usually only on load, create, or destroy.
     * \n Reasonably expensive.
     * @param cmd
     * @param currentFrameOverlap
     * @param previousFrameOverlap
     * @return
     */
    bool updateBuffers(VkCommandBuffer cmd, int32_t currentFrameOverlap, int32_t previousFrameOverlap);

    void dirty() { bufferFramesToUpdate = FRAME_OVERLAP; }

    int32_t bufferFramesToUpdate{0};

public:
    void load();

    void unload();

    bool isLoaded() const { return bIsLoaded; }

    const std::string& getName() { return name; }

    const std::filesystem::path& getGltfPath() { return gltfPath; }

    const std::filesystem::path& getWillmodelPath() { return willmodelPath; }

private:
    bool bIsLoaded{false};

    std::filesystem::path willmodelPath;
    std::filesystem::path gltfPath;
    std::string name;

private:
    std::unordered_map<IRenderable*, RenderableProperties> renderableMap;

    uint32_t getFreePrimitiveIndex();

    std::unordered_set<uint32_t> freePrimitiveIndices{};
    /**
     * The max number of primitives this render object supports at the moment. If a new primitives is made that would exceed this limit, the primitives buffer will be expanded and this value inceased.
     */
    uint32_t currentMaxPrimitiveCount{0};


    uint32_t getFreeInstanceIndex();

    std::unordered_set<uint32_t> freeInstanceIndices{};
    /**
     * The max number of instances this render object supports at the moment. If a new instance is made that would exceed this limit, the instance buffer will be expanded
     */
    uint32_t currentMaxInstanceCount{0};

    std::unordered_map<uint32_t, PrimitiveData> primitiveDataMap{};

public: // IRenderReference
    [[nodiscard]] uint32_t getId() const override { return renderObjectId; }

    bool releaseInstanceIndex(IRenderable* renderable) override;

    std::optional<std::reference_wrapper<const Mesh>> getMeshData(int32_t meshIndex) override;;

private: // IRenderReference
    /**
     * Hash of the file path
     */
    uint32_t renderObjectId{};

public: // Model Rendering API
    GameObject* generateGameObject(const std::string& gameObjectName = "");

    [[nodiscard]] size_t getMeshCount() const { return meshes.size(); }
    [[nodiscard]] bool canDraw() const { return freeInstanceIndices.size() != currentMaxInstanceCount; }
    [[nodiscard]] bool canDrawOpaque() const { return opaqueDrawCommands.size() > 0; }
    [[nodiscard]] bool canDrawTransparent() const { return transparentDrawCommands.size() > 0; }
    [[nodiscard]] const DescriptorBufferUniform& getAddressesDescriptorBuffer() const { return addressesDescriptorBuffer; }
    [[nodiscard]] const DescriptorBufferSampler& getTextureDescriptorBuffer() const { return textureDescriptorBuffer; }
    [[nodiscard]] const DescriptorBufferUniform& getFrustumCullingAddressesDescriptorBuffer() { return frustumCullingDescriptorBuffer; }
    [[nodiscard]] const AllocatedBuffer& getPositionVertexBuffer() const override { return vertexPositionBuffer; }
    [[nodiscard]] const AllocatedBuffer& getPropertyVertexBuffer() const override { return vertexPropertyBuffer; }
    [[nodiscard]] const AllocatedBuffer& getIndexBuffer() const override { return indexBuffer; }

    [[nodiscard]] const AllocatedBuffer& getOpaqueIndirectBuffer(const int32_t currentFrameOverlap) const
    {
        return opaqueDrawIndirectBuffers[currentFrameOverlap];
    }

    [[nodiscard]] size_t getOpaqueDrawIndirectCommandCount() const { return opaqueDrawCommands.size(); }

    [[nodiscard]] const AllocatedBuffer& getTransparentIndirectBuffer(const int32_t currentFrameOverlap) const
    {
        return transparentDrawIndirectBuffers[currentFrameOverlap];
    }

    [[nodiscard]] size_t getTransparentDrawIndirectCommandCount() const { return transparentDrawCommands.size(); }


    void recursiveGenerateGameObject(const RenderNode& renderNode, GameObject* parent);

    /**
     * @param renderable to assign mesh references.
     * @param meshIndex in the RenderObject to generate. \code 0 < n <= meshes.size()\endcode
     * @return true if successfully generated mesh and assigned to gameobject.
     */
    bool generateMesh(IRenderable* renderable, int32_t meshIndex);

private: // Model Parsing
    bool parseGltf(const std::filesystem::path& gltfFilepath, std::vector<MaterialProperties>& materials,
                   std::vector<VertexPosition>& vertexPositions, std::vector<VertexProperty>& vertexProperties, std::vector<uint32_t>& indices);

private: // Model Data
    ResourceManager& resourceManager;

    std::vector<Mesh> meshes{};
    std::vector<BoundingSphere> boundingSpheres{};
    std::vector<RenderNode> renderNodes{};
    std::vector<int32_t> topNodes;

private: // Buffer Data
    std::vector<VkSampler> samplers{};
    std::vector<AllocatedImage> images{};

    std::vector<VkDrawIndexedIndirectCommand> opaqueDrawCommands{};
    std::vector<VkDrawIndexedIndirectCommand> transparentDrawCommands{};

    /**
     * Split vertex Position and Properties to improve GPU cache performance for passes that only need position (shadow pass, depth prepass), assuming these passes don't need the other properties of course
     */
    AllocatedBuffer vertexPositionBuffer{};
    AllocatedBuffer vertexPropertyBuffer{};
    AllocatedBuffer indexBuffer{};
    AllocatedBuffer opaqueDrawIndirectBuffers[FRAME_OVERLAP]{};
    AllocatedBuffer transparentDrawIndirectBuffers[FRAME_OVERLAP]{};

    // addresses
    AllocatedBuffer addressBuffers[FRAME_OVERLAP]{};
    //  the actual buffers
    AllocatedBuffer materialBuffer{};
    AllocatedBuffer primitiveDataBuffers[FRAME_OVERLAP]{};
    AllocatedBuffer modelMatrixBuffers[FRAME_OVERLAP]{};

    DescriptorBufferUniform addressesDescriptorBuffer;
    DescriptorBufferSampler textureDescriptorBuffer;

    AllocatedBuffer meshBoundsBuffer{};
    AllocatedBuffer opaqueCullingAddressBuffers[FRAME_OVERLAP]{};
    AllocatedBuffer transparentCullingAddressBuffers[FRAME_OVERLAP]{};
    DescriptorBufferUniform frustumCullingDescriptorBuffer;
};
}

#endif //MODEL_H
