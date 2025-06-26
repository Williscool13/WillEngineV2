//
// Created by William on 2025-06-17.
//

#ifndef RENDER_OBJECT_H
#define RENDER_OBJECT_H

#include "render_reference.h"
#include "engine/renderer/resource_manager.h"

namespace will_engine
{
class IComponentContainer;
}

namespace will_engine::renderer
{
/**
 * Render Objects are persistent class representations of GLTF files. They always exist and their lifetime is managed through \code AssetManager\endcode.
 * \n The Render Object can be loaded/unloaded at runtime to avoid unnecessary GPU allocations.
 */
class RenderObject : public IRenderReference
{
public:
    RenderObject() = delete;

    RenderObject(ResourceManager& resourceManager, const RenderObjectInfo& renderObjectInfo);

    ~RenderObject() override;

public: // Resource management
    virtual void load() = 0;

    virtual void unload() = 0;

public: // Buffer update
    /**
     * Called every frame to instruct the render object to update their buffers.
     * This is deferred until after the game loop so that all changes execute at the same time.
     * @param cmd
     * @param currentFrameOverlap guaranteed to never exceed `FRAME_OVERLAP`
     * @param previousFrameOverlap guaranteed to never exceed `FRAME_OVERLAP`
     */
    virtual void update(VkCommandBuffer cmd, int32_t currentFrameOverlap, int32_t previousFrameOverlap) = 0;

    /**
     * Called whenever either meshes are loaded (init) or a model matrix is modified (gameplay). It is the responsibility of the RenderObject
     * to correctly keep track of what needs to be updated over multiple frames.
     */
    virtual void dirty() = 0;

public: // Engine API
    /**
     * Attempt to generate all mesh components associated with the RenderObject, assigning the appropriate transforms throughout the hierarchy
     * @param container
     * @param transform
     */
    virtual void generateMeshComponents(IComponentContainer* container, const Transform& transform) = 0;

    /**
     * Generates a single mesh from the RenderObject, the transform will always be Identity.
     * @param renderable
     * @param meshIndex
     * @return
     */
    virtual void generateMesh(IRenderable* renderable, int32_t meshIndex) = 0;

    virtual size_t getMeshCount() const = 0;

    virtual bool canDraw() const = 0;

    virtual bool canDrawOpaque() const = 0;

    virtual bool canDrawTransparent() const = 0;

    virtual const DescriptorBufferUniform* getVisibilityDescriptorBuffer() const = 0;

    virtual const DescriptorBufferUniform* getAddressesDescriptorBuffer() const = 0;

    virtual const DescriptorBufferSampler* getTextureDescriptorBuffer() const = 0;

    virtual const DescriptorBufferUniform* getFrustumCullingAddressesDescriptorBuffer() const = 0;

    VkBuffer getPositionVertexBuffer() const override = 0;

    VkBuffer getPropertyVertexBuffer() const override = 0;

    VkBuffer getIndexBuffer() const override = 0;

    virtual VkBuffer getOpaqueIndirectBuffer(int32_t currentFrameOverlap) const = 0;

    virtual VkBuffer getTransparentIndirectBuffer(int32_t currentFrameOverlap) const = 0;

public: // RenderReference
    uint32_t getId() const override { return renderObjectInfo.id; }

public:
    const RenderObjectInfo& getRenderObjectInfo() { return renderObjectInfo; }
    const std::string& getName() const { return renderObjectInfo.name; }
    bool isLoaded() const { return bIsLoaded; }

protected:
    ResourceManager& resourceManager;
    RenderObjectInfo renderObjectInfo;

    bool bIsLoaded{false};

#if WILL_ENGINE_DEBUG

public:
    virtual const std::vector<ImageResourcePtr>& debugGetImages() = 0;
#endif
};
}

#endif //RENDER_OBJECT_H
