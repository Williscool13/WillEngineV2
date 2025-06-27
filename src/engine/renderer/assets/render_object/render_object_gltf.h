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
    uint32_t modelIndex;
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

    uint32_t getFreeInstanceIndex();

    std::vector<uint32_t> freeInstanceIndices{};
    /**
     * The max number of primitives this render object supports at the moment. If a new primitives is made that would exceed this limit, the primitives buffer will be expanded and this value inceased.
     */
    uint32_t currentMaxInstanceCount{DEFAULT_RENDER_OBJECT_INSTANCE_COUNT};


    uint32_t getFreeModelIndex();

    std::vector<uint32_t> freeModelIndex{};
    /**
     * The max number of instances this render object supports at the moment. If a new instance is made that would exceed this limit, the instance buffer will be expanded
     */
    uint32_t currentMaxModelCount{DEFAULT_RENDER_OBJECT_MODEL_COUNT};

    std::vector<InstanceData> instanceData;

public: // IRenderReference
    /**
     * Releases the instance index and all associated primitives with that instance
     * @param renderable
     * @return
     */
    bool releaseInstanceIndex(IRenderable* renderable) override;

    std::vector<Primitive> getPrimitives(int32_t meshIndex) override;

public: // Model Rendering API
    size_t getMeshCount() const override { return meshes.size(); }
    bool canDraw() const override { return freeModelIndex.size() != currentMaxModelCount; }
    const DescriptorBufferUniform* getAddressesDescriptorBuffer() const override { return addressesDescriptorBuffer.get(); }
    const DescriptorBufferSampler* getTextureDescriptorBuffer() const override { return textureDescriptorBuffer.get(); }
    const DescriptorBufferUniform* getVisibilityPassDescriptorBuffer() const override { return visibilityPassDescriptorBuffer.get(); }

    VkBuffer getPositionVertexBuffer() const override { return vertexPositionBuffer ? vertexPositionBuffer->buffer : VK_NULL_HANDLE; }
    VkBuffer getPropertyVertexBuffer() const override { return vertexPositionBuffer ? vertexPropertyBuffer->buffer : VK_NULL_HANDLE; }
    VkBuffer getIndexBuffer() const override { return indexBuffer ? indexBuffer->buffer : VK_NULL_HANDLE; }

    VkBuffer getOpaqueIndirectBuffer(const int32_t currentFrameOverlap) const override { return compactOpaqueDrawBuffers[currentFrameOverlap] ? compactOpaqueDrawBuffers[currentFrameOverlap]->buffer : VK_NULL_HANDLE; }
    VkBuffer getTransparentIndirectBuffer(const int32_t currentFrameOverlap) const override { return compactTransparentDrawBuffers[currentFrameOverlap] ? compactTransparentDrawBuffers[currentFrameOverlap]->buffer : VK_NULL_HANDLE; }
    VkBuffer getDrawCountBuffer(const int32_t currentFrameOverlap) const override { return countBuffers[currentFrameOverlap] ? countBuffers[currentFrameOverlap]->buffer : VK_NULL_HANDLE; }
    VkDeviceSize getDrawCountOpaqueOffset() const override { return offsetof(IndirectCount, opaqueCount); }
    VkDeviceSize getDrawCountTransparentOffset() const override { return offsetof(IndirectCount, transparentCount); }
    uint32_t getMaxDrawCount() const override { return currentMaxInstanceCount; }
    void resetDrawCount(VkCommandBuffer cmd, int32_t currentFrameOverlap) const override;

    void generateMeshComponents(IComponentContainer* container, const Transform& transform) override;

    void generateMesh(IRenderable* renderable, int32_t meshIndex) override;

    void recursiveGenerate(const RenderNode& renderNode, IComponentContainer* container, const Transform& parentTransform);

private: // Model Parsing
    bool parseGltf(const std::filesystem::path& gltfFilepath, std::vector<MaterialProperties>& materials, std::vector<VertexPosition>& vertexPositions, std::vector<VertexProperty>& vertexProperties,
                   std::vector<uint32_t>& indices, std::vector<Primitive>& primitives);

private: // Model Data
    std::vector<Mesh> meshes{};
    std::vector<RenderNode> renderNodes{};
    std::vector<int32_t> topNodes{};

public: // Buffer Data
    std::vector<SamplerPtr> samplers{};
    // todo: refactor this to use the new TextureResource class
    std::vector<ImageResourcePtr> images{};

    /**
     * Split vertex Position and Properties to improve GPU cache performance for passes that only need position (shadow pass, depth prepass), assuming these passes don't need the other properties of course
     */
    BufferPtr vertexPositionBuffer{};
    BufferPtr vertexPropertyBuffer{};
    BufferPtr indexBuffer{};
    BufferPtr primitiveBuffer{};

    DescriptorBufferSamplerPtr textureDescriptorBuffer{};

    // Main Pass
    DescriptorBufferUniformPtr addressesDescriptorBuffer{};
    std::array<BufferPtr, FRAME_OVERLAP> addressBuffers{};

    // todo: Material buffer probably also needs to be multi-buffered if it changes at runtime, which it probably should be allowed to
    BufferPtr materialBuffer{};
    std::array<BufferPtr, FRAME_OVERLAP> instanceDataBuffer{};
    std::array<BufferPtr, FRAME_OVERLAP> modelMatrixBuffers{};

    // Visibility Pass
    DescriptorBufferUniformPtr visibilityPassDescriptorBuffer{};
    std::array<BufferPtr, FRAME_OVERLAP> visibilityPassBuffers{};
    // Draw buffer written to by the visibility pass
    std::array<BufferPtr, FRAME_OVERLAP> compactOpaqueDrawBuffers{};
    std::array<BufferPtr, FRAME_OVERLAP> compactTransparentDrawBuffers{};
    std::array<BufferPtr, FRAME_OVERLAP> countBuffers{};

#if WILL_ENGINE_DEBUG
    std::vector<Primitive> debugPrimitives;

    const std::vector<ImageResourcePtr>& debugGetImages() override { return images; }
#endif
};
}

#endif //MODEL_H
