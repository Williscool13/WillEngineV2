//
// Created by William on 2024-12-26.
//

#ifndef JOLT_DEBUG_RENDERER_H
#define JOLT_DEBUG_RENDERER_H

#include <src/physics/physics.h>

namespace will_engine::debug_renderer
{
class DebugRenderer;
}

namespace will_engine::physics
{
class JoltDebugRenderer final : public JPH::DebugRenderer
{
public:
    JoltDebugRenderer();

    ~JoltDebugRenderer() override;

    void DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor) override;

    void DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow inCastShadow) override;

    Batch CreateTriangleBatch(const Triangle* inTriangles, int inTriangleCount) override;

    Batch CreateTriangleBatch(const Vertex* inVertices, int inVertexCount, const JPH::uint32* inIndices, int inIndexCount) override;

    void DrawGeometry(JPH::RMat44Arg inModelMatrix, const JPH::AABox& inWorldSpaceBounds, float inLODScaleSq, JPH::ColorArg inModelColor,
                      const GeometryRef& inGeometry, ECullMode inCullMode,
                      ECastShadow inCastShadow, EDrawMode inDrawMode) override;

    void DrawText3D(JPH::RVec3Arg inPosition, const JPH::string_view& inString, JPH::ColorArg inColor, float inHeight) override;

    class BatchImpl final : public JPH::RefTargetVirtual
    {
    public:
        JPH_OVERRIDE_NEW_DELETE

        virtual void AddRef() override { ++mRefCount; }
        virtual void Release() override { if (--mRefCount == 0) delete this; }

        JPH::Array<Triangle> mTriangles;

    private:
        std::atomic<uint32_t> mRefCount = 0;
    };

    /// Last provided camera position
    JPH::RVec3 mCameraPos;
    bool mCameraPosSet = false;
};
}

#endif //JOLT_DEBUG_RENDERER_H
