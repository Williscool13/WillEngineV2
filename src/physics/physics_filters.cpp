//
// Created by William on 2024-12-06.
//

#include "physics_filters.h"
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>

BPLayerInterfaceImpl::BPLayerInterfaceImpl()
{
    // Map each object layer to a broad phase layer
    mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
    mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
    mObjectToBroadPhase[Layers::TERRAIN] = BroadPhaseLayers::NON_MOVING;
}

JPH::uint BPLayerInterfaceImpl::GetNumBroadPhaseLayers() const
{
    return BroadPhaseLayers::NUM_LAYERS;
}

JPH::BroadPhaseLayer BPLayerInterfaceImpl::GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const
{
    JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
    return mObjectToBroadPhase[inLayer];
}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
const char* BPLayerInterfaceImpl::GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const {
    switch ((JPH::BroadPhaseLayer::Type)inLayer) {
        case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING: return "NON_MOVING";
        case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING: return "MOVING";
        default: JPH_ASSERT(false); return "INVALID";
    }
}
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

bool ObjectVsBroadPhaseLayerFilterImpl::ShouldCollide(const JPH::ObjectLayer inLayer1, const JPH::BroadPhaseLayer inLayer2) const
{
    switch (inLayer1) {
        case Layers::NON_MOVING:
            return inLayer2 == BroadPhaseLayers::MOVING;
        case Layers::MOVING:
            return true;
        case Layers::TERRAIN:
            return inLayer2 == BroadPhaseLayers::MOVING;
        default:
            JPH_ASSERT(false);
            return false;
    }
}

bool ObjectLayerPairFilterImpl::ShouldCollide(JPH::ObjectLayer inLayer1, JPH::ObjectLayer inLayer2) const
{
    // If either object is NON_MOVING or TERRAIN, they only collide with MOVING objects
    if (inLayer1 == Layers::NON_MOVING || inLayer1 == Layers::TERRAIN ||
        inLayer2 == Layers::NON_MOVING || inLayer2 == Layers::TERRAIN)
    {
        return (inLayer1 == Layers::MOVING || inLayer2 == Layers::MOVING);
    }

    return true;
}
