//
// Created by William on 2024-12-26.
//

#ifndef DEBUG_RENDERER_H
#define DEBUG_RENDERER_H

#include <src/physics/physics.h>
#include <Jolt/Renderer/DebugRenderer.h>

namespace will_engine::physics
{
class JoltDebugRenderer final : public JPH::DebugRenderer
{
public:
    JoltDebugRenderer();

    ~JoltDebugRenderer() override;

    void DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor) override;

    void DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor, ECastShadow inCastShadow = ECastShadow::Off) override;

    Batch CreateTriangleBatch(const Triangle* inTriangles, int inTriangleCount) override;

    Batch CreateTriangleBatch(const Vertex* inVertices, int inVertexCount, const JPH::uint32* inIndices, int inIndexCount) override;

    void DrawGeometry(JPH::RMat44Arg inModelMatrix, const JPH::AABox& inWorldSpaceBounds, float inLODScaleSq, JPH::ColorArg inModelColor, const GeometryRef& inGeometry, ECullMode inCullMode,
                      ECastShadow inCastShadow, EDrawMode inDrawMode) override;

    void DrawText3D(JPH::RVec3Arg inPosition, const JPH::string_view& inString, JPH::ColorArg inColor, float inHeight) override;
};
}

#endif //DEBUG_RENDERER_H
