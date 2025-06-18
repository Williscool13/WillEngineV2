//
// Created by William on 2025-01-24.
//

#ifndef MODEL_H
#define MODEL_H

#include <filesystem>
#include <unordered_set>
#include <fastgltf/types.hpp>

#include "render_object.h"
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
class RenderObjectGltf final : public RenderObject
{
public:
    RenderObjectGltf(ResourceManager& resourceManager, const RenderObjectInfo& renderObjectInfo);

    ~RenderObjectGltf() override;

    void update(VkCommandBuffer cmd, int32_t currentFrameOverlap, int32_t previousFrameOverlap) override;

    void dirty() override;

    int32_t bufferFramesToUpdate{0};

    /**
     * Uploads all mesh-related buffer data to the GPU. If buffers require resizing, this will also regenerate them.
     * Only done if the render object is marked as dirty, which only happens on `load`, `destroy`, or `generate`
     * @param cmd
     * @param currentFrameOverlap
     * @return
     */
    void updateBuffers(VkCommandBuffer cmd, int32_t currentFrameOverlap);

public:
    void load() override;

    void unload() override;

private:
    /**
     * A map between a renderable and its model matrix
     */
    std::unordered_map<IRenderable*, RenderableProperties> renderableMap{};

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
    /**
     * Releases the instance index and all associated primitives with that instance
     * @param renderable
     * @return
     */
    bool releaseInstanceIndex(IRenderable* renderable) override;

    std::optional<std::reference_wrapper<const Mesh> > getMeshData(int32_t meshIndex) override;;

public: // Model Rendering API
    size_t getMeshCount() const override { return meshes.size(); }
    bool canDraw() const override { return freeInstanceIndices.size() != currentMaxInstanceCount; }
    bool canDrawOpaque() const override { return opaqueDrawCommands.size() > 0; }
    bool canDrawTransparent() const override { return transparentDrawCommands.size() > 0; }
    const DescriptorBufferUniform* getAddressesDescriptorBuffer() const override { return addressesDescriptorBuffer.get(); }
    const DescriptorBufferSampler* getTextureDescriptorBuffer() const override { return textureDescriptorBuffer.get(); }
    const DescriptorBufferUniform* getFrustumCullingAddressesDescriptorBuffer() const override { return frustumCullingDescriptorBuffer.get(); }
    VkBuffer getPositionVertexBuffer() const override { return vertexPositionBuffer ? vertexPositionBuffer->buffer : VK_NULL_HANDLE; }
    VkBuffer getPropertyVertexBuffer() const override { return vertexPositionBuffer ? vertexPropertyBuffer->buffer : VK_NULL_HANDLE; }
    VkBuffer getIndexBuffer() const override { return indexBuffer ? indexBuffer->buffer : VK_NULL_HANDLE; }

    VkBuffer getOpaqueIndirectBuffer(const int32_t currentFrameOverlap) const override
    {
        return opaqueDrawIndirectBuffers[currentFrameOverlap]->buffer;
    }

    size_t getOpaqueDrawIndirectCommandCount() const override { return opaqueDrawCommands.size(); }

    VkBuffer getTransparentIndirectBuffer(const int32_t currentFrameOverlap) const override
    {
        return transparentDrawIndirectBuffers[currentFrameOverlap]->buffer;
    }

    size_t getTransparentDrawIndirectCommandCount() const override { return transparentDrawCommands.size(); }

    void generateMeshComponents(IComponentContainer* container, const Transform& transform) override;

    void generateMesh(IRenderable* renderable, int32_t meshIndex) override;

    void recursiveGenerate(const RenderNode& renderNode, IComponentContainer* container, const Transform& parentTransform);

private: // Model Parsing
    bool parseGltf(const std::filesystem::path& gltfFilepath, std::vector<MaterialProperties>& materials,
                   std::vector<VertexPosition>& vertexPositions, std::vector<VertexProperty>& vertexProperties, std::vector<uint32_t>& indices);

private: // Model Data
    std::vector<Mesh> meshes{};
    std::vector<BoundingSphere> boundingSpheres{};
    std::vector<RenderNode> renderNodes{};
    std::vector<int32_t> topNodes{};

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

#if WILL_ENGINE_DEBUG
    const std::vector<ImageResourcePtr>& debugGetImages() override { return images; }
#endif
};
}

#endif //MODEL_H
