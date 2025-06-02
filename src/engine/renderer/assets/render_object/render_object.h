//
// Created by William on 2025-01-24.
//

#ifndef MODEL_H
#define MODEL_H

#include <filesystem>
#include <unordered_set>

#include "render_object_types.h"
#include "render_reference.h"
#include "engine/core/game_object/renderable.h"
#include "engine/renderer/renderer_constants.h"
#include "engine/renderer/resource_manager.h"
#include "engine/renderer/resources/buffer.h"
#include "engine/renderer/resources/descriptor_buffer/descriptor_buffer_sampler.h"
#include "engine/renderer/resources/descriptor_buffer/descriptor_buffer_uniform.h"
#include "engine/renderer/resources/resources_fwd.h"


namespace will_engine
{
class IComponentContainer;
}

namespace will_engine::game_object
{
class GameObject;
}

namespace will_engine::renderer
{
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

    /**
     * Will mark all renderables as dirty.
     */
    void dirty();

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

    /**
     * Releases the instance index and all associated primitives with that instance
     * @param renderable
     * @return
     */
    bool releaseInstanceIndex(IRenderable* renderable) override;

    std::optional<std::reference_wrapper<const Mesh>> getMeshData(int32_t meshIndex) override;;

private: // IRenderReference
    /**
     * Hash of the file path
     */
    uint32_t renderObjectId{};

public: // Model Rendering API
    bool generateMeshComponents(IComponentContainer* container, const Transform& transform = Transform::Identity);

    [[nodiscard]] size_t getMeshCount() const { return meshes.size(); }
    [[nodiscard]] bool canDraw() const { return freeInstanceIndices.size() != currentMaxInstanceCount; }
    [[nodiscard]] bool canDrawOpaque() const { return opaqueDrawCommands.size() > 0; }
    [[nodiscard]] bool canDrawTransparent() const { return transparentDrawCommands.size() > 0; }
    [[nodiscard]] const DescriptorBufferUniform* getAddressesDescriptorBuffer() const { return addressesDescriptorBuffer.get(); }
    [[nodiscard]] const DescriptorBufferSampler* getTextureDescriptorBuffer() const { return textureDescriptorBuffer.get(); }
    [[nodiscard]] const DescriptorBufferUniform* getFrustumCullingAddressesDescriptorBuffer() const { return frustumCullingDescriptorBuffer.get(); }
    [[nodiscard]] VkBuffer getPositionVertexBuffer() const override { return vertexPositionBuffer ? vertexPositionBuffer->buffer : VK_NULL_HANDLE; }
    [[nodiscard]] VkBuffer getPropertyVertexBuffer() const override { return vertexPositionBuffer ? vertexPropertyBuffer->buffer : VK_NULL_HANDLE; }
    [[nodiscard]] VkBuffer getIndexBuffer() const override          { return indexBuffer ? indexBuffer->buffer : VK_NULL_HANDLE; }

    [[nodiscard]] VkBuffer getOpaqueIndirectBuffer(const int32_t currentFrameOverlap) const
    {
        return opaqueDrawIndirectBuffers[currentFrameOverlap]->buffer;
    }

    [[nodiscard]] size_t getOpaqueDrawIndirectCommandCount() const { return opaqueDrawCommands.size(); }

    [[nodiscard]] VkBuffer getTransparentIndirectBuffer(const int32_t currentFrameOverlap) const
    {
        return transparentDrawIndirectBuffers[currentFrameOverlap]->buffer;
    }

    [[nodiscard]] size_t getTransparentDrawIndirectCommandCount() const { return transparentDrawCommands.size(); }


    void recursiveGenerate(const RenderNode& renderNode, IComponentContainer* container);
    void recursiveGenerate(const RenderNode& renderNode, IComponentContainer* container, const Transform& parentTransform);

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

public: // Buffer Data
    std::vector<SamplerPtr> samplers{};
    // todo: refactor this to use the new TextureResource class
    std::vector<ImageResourcePtr> images{};

    std::vector<VkDrawIndexedIndirectCommand> opaqueDrawCommands{};
    std::vector<VkDrawIndexedIndirectCommand> transparentDrawCommands{};

    /**
     * Split vertex Position and Properties to improve GPU cache performance for passes that only need position (shadow pass, depth prepass), assuming these passes don't need the other properties of course
     */
    BufferPtr vertexPositionBuffer{};
    BufferPtr vertexPropertyBuffer{};
    BufferPtr indexBuffer{};
    std::array<BufferPtr, FRAME_OVERLAP> opaqueDrawIndirectBuffers{};
    std::array<BufferPtr, FRAME_OVERLAP> transparentDrawIndirectBuffers{};

    // addresses
    std::array<BufferPtr, FRAME_OVERLAP> addressBuffers{};
    //  the actual buffers
    // todo: Material buffer probably also needs to be multi-buffered if it changes at runtime, which it probably should be allowed to
    BufferPtr materialBuffer{};
    std::array<BufferPtr, FRAME_OVERLAP> primitiveDataBuffers{};
    std::array<BufferPtr, FRAME_OVERLAP> modelMatrixBuffers{};

    DescriptorBufferUniformPtr addressesDescriptorBuffer{};
    DescriptorBufferSamplerPtr textureDescriptorBuffer{};

    BufferPtr meshBoundsBuffer{};
    std::array<BufferPtr, FRAME_OVERLAP> opaqueCullingAddressBuffers{};
    std::array<BufferPtr, FRAME_OVERLAP> transparentCullingAddressBuffers{};
    DescriptorBufferUniformPtr frustumCullingDescriptorBuffer{};
};
}

#endif //MODEL_H
