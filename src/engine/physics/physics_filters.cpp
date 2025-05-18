//
// Created by William on 2024-12-06.
//

#include "physics_filters.h"
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>

namespace will_engine::physics
{
BPLayerInterfaceImpl::BPLayerInterfaceImpl()
{
    // Map each object layer to a broad phase layer
    mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::STATIC;
    mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::DYNAMIC;
    mObjectToBroadPhase[Layers::PLAYER] = BroadPhaseLayers::DYNAMIC;
    mObjectToBroadPhase[Layers::TERRAIN] = BroadPhaseLayers::STATIC;
    mObjectToBroadPhase[Layers::SENSOR] = BroadPhaseLayers::SENSOR;
    mObjectToBroadPhase[Layers::PROJECTILE] = BroadPhaseLayers::PROJECTILE;
    mObjectToBroadPhase[Layers::DEBRIS] = BroadPhaseLayers::DYNAMIC;
    mObjectToBroadPhase[Layers::CHARACTER] = BroadPhaseLayers::DYNAMIC;
    mObjectToBroadPhase[Layers::UI_RAYCAST] = BroadPhaseLayers::SENSOR;
    mObjectToBroadPhase[Layers::VEHICLE] = BroadPhaseLayers::DYNAMIC;
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
    // No physical collision
    if (inLayer1 == Layers::SENSOR || inLayer1 == Layers::UI_RAYCAST) {
        return false;
    }

    // NON_MOVING and TERRAIN objects don't collide with other STATIC objects
    if ((inLayer1 == Layers::NON_MOVING || inLayer1 == Layers::TERRAIN) && inLayer2 == BroadPhaseLayers::STATIC) {
        return false;
    }

    return true;
}

bool ObjectLayerPairFilterImpl::ShouldCollide(JPH::ObjectLayer inLayer1, JPH::ObjectLayer inLayer2) const
{
    if (inLayer1 == Layers::UI_RAYCAST || inLayer2 == Layers::UI_RAYCAST)
        return false;

    if (inLayer1 == Layers::SENSOR || inLayer2 == Layers::SENSOR)
        return false;

    return true;
}
}
