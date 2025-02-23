//
// Created by William on 2025-02-22.
//

#ifndef RIGID_BODY_COMPONENT_H
#define RIGID_BODY_COMPONENT_H

#include <string_view>

#include <json/json.hpp>
#include <JoltPhysics/Jolt/Jolt.h>
#include <JoltPhysics/Jolt/Physics/Body/BodyID.h>


#include "src/core/game_object/component_container.h"
#include "src/core/game_object/components/component.h"
#include "src/physics/physics_body.h"
#include "src/physics/physics_types.h"

namespace will_engine
{
class ITransformable;
}

namespace will_engine::components
{
class RigidBodyComponent : public Component, public IPhysicsBody
{
public:
    explicit RigidBodyComponent(const std::string& name = "");

    ~RigidBodyComponent() override;

    static constexpr auto TYPE = "RigidBodyComponent";

    static std::string_view getStaticType()
    {
        return TYPE;
    }

    std::string_view getComponentType() override { return TYPE; }

    void beginPlay() override;

    void update(float deltaTime) override;

    void releaseRigidBody();

    void beginDestroy() override;

    void onEnable() override;

    void onDisable() override;

    void setOwner(IComponentContainer* owner) override;

    // IPhysicsBody
    void setGameTransformFromPhysics(const glm::vec3& position, const glm::quat& rotation) override;

    void setPhysicsTransformFromGame(const glm::vec3& position, const glm::quat& rotation) override;

    glm::vec3 getGlobalPosition() override;

    glm::quat getGlobalRotation() override;

    void setPhysicsBodyId(const JPH::BodyID bodyId) override { this->bodyId = bodyId; }

    JPH::BodyID getPhysicsBodyId() const override { return bodyId; }

public: // Serialization
    void serialize(ordered_json& j) override;

    void deserialize(ordered_json& j) override;

    physics::PhysicsProperties deserializedPhysicsProperties{};

public: // Editor Tools
    void updateRenderImgui() override;

private:
    ITransformable* transformableOwner{nullptr};

    JPH::BodyID bodyId{JPH::BodyID::cMaxBodyIndex};

};
} // namespace will_engine::components

#endif // RIGID_BODY_COMPONENT_H
