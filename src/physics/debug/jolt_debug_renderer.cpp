//
// Created by William on 2024-12-26.
//

#include "jolt_debug_renderer.h"

#include "src/physics/physics_utils.h"
#include "src/renderer/debug/debug_renderer.h"


will_engine::physics::JoltDebugRenderer::JoltDebugRenderer()
{
    Initialize();
}

will_engine::physics::JoltDebugRenderer::~JoltDebugRenderer() {}

void will_engine::physics::JoltDebugRenderer::DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, const JPH::ColorArg inColor)
{
    debug_renderer::DebugRenderer::drawLine(PhysicsUtils::toGLM(inFrom), PhysicsUtils::toGLM(inTo), PhysicsUtils::toGLM(inColor));
}

void will_engine::physics::JoltDebugRenderer::DrawTriangle(JPH::RVec3Arg inV1, JPH::RVec3Arg inV2, JPH::RVec3Arg inV3, JPH::ColorArg inColor,
                                                           ECastShadow inCastShadow)
{
    // Implement this
}

JPH::DebugRenderer::Batch will_engine::physics::JoltDebugRenderer::CreateTriangleBatch(const Triangle* inTriangles, int inTriangleCount)
{
    // Implement This

    return {};
}

JPH::DebugRenderer::Batch will_engine::physics::JoltDebugRenderer::CreateTriangleBatch(const Vertex* inVertices, int inVertexCount,
                                                                                       const JPH::uint32* inIndices, int inIndexCount)
{
    // Implement This
    return {};
}

void will_engine::physics::JoltDebugRenderer::DrawGeometry(JPH::RMat44Arg inModelMatrix, const JPH::AABox& inWorldSpaceBounds, float inLODScaleSq,
                                                           JPH::ColorArg inModelColor, const GeometryRef& inGeometry, ECullMode inCullMode,
                                                           ECastShadow inCastShadow, EDrawMode inDrawMode)
{
    // Implement This
}

void will_engine::physics::JoltDebugRenderer::DrawText3D(JPH::RVec3Arg inPosition, const JPH::string_view& inString, JPH::ColorArg inColor,
                                                         float inHeight)
{
    // Do not implement this
}
