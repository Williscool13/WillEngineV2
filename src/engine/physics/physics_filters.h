//
// Created by William on 2024-12-06.
//

#ifndef PHYSICS_FILTERS_H
#define PHYSICS_FILTERS_H

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>

namespace will_engine::physics
{
namespace Layers
{
    static constexpr JPH::ObjectLayer NON_MOVING{0};
    static constexpr JPH::ObjectLayer MOVING{1};
    static constexpr JPH::ObjectLayer PLAYER{2};
    static constexpr JPH::ObjectLayer TERRAIN{3};
    static constexpr JPH::ObjectLayer SENSOR{4};        // Non-Physical Volumes / Triggers
    static constexpr JPH::ObjectLayer PROJECTILE{5};
    static constexpr JPH::ObjectLayer DEBRIS{6};
    static constexpr JPH::ObjectLayer CHARACTER{7};
    static constexpr JPH::ObjectLayer UI_RAYCAST{8};    // Diegetic UI / Non-HUD UI
    static constexpr JPH::ObjectLayer VEHICLE{9};
    static constexpr JPH::ObjectLayer NUM_LAYERS{10};

    inline const char* layerNames[] = {
        "Non-Moving", "Moving", "Player", "Terrain", "Sensor",
        "Projectile", "Debris", "Character", "UI-Raycast", "Vehicle"
    };
}

namespace BroadPhaseLayers
{
    static constexpr JPH::BroadPhaseLayer STATIC(0);
    static constexpr JPH::BroadPhaseLayer DYNAMIC(1);
    static constexpr JPH::BroadPhaseLayer SENSOR(2);
    static constexpr JPH::BroadPhaseLayer PROJECTILE(3);
    static constexpr JPH::uint NUM_LAYERS(4);
}

class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
{
public:
    /**
     * Matches Jolt naming convention
     */
    BPLayerInterfaceImpl();

    [[nodiscard]] JPH::uint GetNumBroadPhaseLayers() const override;

    [[nodiscard]] JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override;

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* BPLayerInterfaceImpl::GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const {
        switch ((JPH::BroadPhaseLayer::Type)inLayer) {
            case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING: return "NON_MOVING";
            case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING: return "MOVING";
            default: JPH_ASSERT(false); return "INVALID";
        }
    }
#endif

private:
    /**
     * Match Jolt naming convention
     */
    JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS]{};
};


class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter
{
public:
    /**
     * Should do physics checks against specified BPL?
     * @param inLayer1
     * @param inLayer2
     * @return
     */
    [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override;
};

class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter
{
public:
    /**
     * Should do physics checks against specified layer?
     * @param inLayer1
     * @param inLayer2
     * @return
     */
    [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::ObjectLayer inLayer2) const override;
};

class PlayerCollisionFilter final : public JPH::ObjectLayerFilter
{
public:
    [[nodiscard]] bool ShouldCollide(const JPH::ObjectLayer inLayer) const override
    {
        return inLayer != Layers::PLAYER;
    }
};
}

#endif //PHYSICS_FILTERS_H
