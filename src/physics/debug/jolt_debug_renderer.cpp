//
// Created by William on 2024-12-26.
//

#ifdef JPH_DEBUG_RENDERER

#include "jolt_debug_renderer.h"

#include "src/physics/physics_utils.h"
#include "../../renderer/pipelines/debug/debug_renderer.h"


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
    debug_renderer::DebugRenderer::drawTriangle(PhysicsUtils::toGLM(inV1), PhysicsUtils::toGLM(inV2), PhysicsUtils::toGLM(inV3),
                                                PhysicsUtils::toGLM(inColor));
}

JPH::DebugRenderer::Batch will_engine::physics::JoltDebugRenderer::CreateTriangleBatch(const Triangle* inTriangles, int inTriangleCount)
{
    // Copied from JPH::DebugRendererSimple
    const auto batch = new BatchImpl;
    if (inTriangles == nullptr || inTriangleCount == 0)
        return batch;

    batch->mTriangles.assign(inTriangles, inTriangles + inTriangleCount);
    return batch;
}

JPH::DebugRenderer::Batch will_engine::physics::JoltDebugRenderer::CreateTriangleBatch(const Vertex* inVertices, int inVertexCount,
                                                                                       const JPH::uint32* inIndices, int inIndexCount)
{
    // Copied from JPH::DebugRendererSimple
    const auto batch = new BatchImpl;
    if (inVertices == nullptr || inVertexCount == 0 || inIndices == nullptr || inIndexCount == 0)
        return batch;

    // Convert indexed triangle list to triangle list
    batch->mTriangles.resize(inIndexCount / 3);
    for (size_t t = 0; t < batch->mTriangles.size(); ++t) {
        Triangle& triangle = batch->mTriangles[t];
        triangle.mV[0] = inVertices[inIndices[t * 3 + 0]];
        triangle.mV[1] = inVertices[inIndices[t * 3 + 1]];
        triangle.mV[2] = inVertices[inIndices[t * 3 + 2]];
    }

    return batch;
}

void will_engine::physics::JoltDebugRenderer::DrawGeometry(JPH::RMat44Arg inModelMatrix, const JPH::AABox& inWorldSpaceBounds, float inLODScaleSq,
                                                           JPH::ColorArg inModelColor, const GeometryRef& inGeometry, ECullMode inCullMode,
                                                           ECastShadow inCastShadow, EDrawMode inDrawMode)
{
    // cull mode is always ignored (never cull)
    // cast shadow is always ignored (never cast shadows)
    // draw mode is always ignored (always wireframe)

    // Copied from JPH::DebugRendererSimple
    // Figure out which LOD to use (default to LOD 0)
    const LOD* lod = inGeometry->mLODs.data();
    if (mCameraPosSet)
        lod = &inGeometry->GetLOD(JPH::Vec3(mCameraPos), inWorldSpaceBounds, inLODScaleSq);

    // Draw the batch
    const BatchImpl* batch = static_cast<const BatchImpl*>(lod->mTriangleBatch.GetPtr());
    for (const Triangle& triangle : batch->mTriangles) {
        const JPH::RVec3 v0 = inModelMatrix * JPH::Vec3(triangle.mV[0].mPosition);
        const JPH::RVec3 v1 = inModelMatrix * JPH::Vec3(triangle.mV[1].mPosition);
        const JPH::RVec3 v2 = inModelMatrix * JPH::Vec3(triangle.mV[2].mPosition);
        const JPH::Color color = inModelColor * triangle.mV[0].mColor;

        switch (inDrawMode) {
            case EDrawMode::Wireframe:
                DrawLine(v0, v1, color);
                DrawLine(v1, v2, color);
                DrawLine(v2, v0, color);
                break;

            case EDrawMode::Solid:
                DrawTriangle(v0, v1, v2, color, inCastShadow);
                break;
        }
    }
}

void will_engine::physics::JoltDebugRenderer::DrawText3D(JPH::RVec3Arg inPosition, const JPH::string_view& inString, JPH::ColorArg inColor,
                                                         float inHeight)
{
    // Do not implement this
}

#endif // JPH_DEBUG_RENDERER
