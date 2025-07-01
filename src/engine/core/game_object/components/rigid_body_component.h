//
// Created by William on 2025-02-22.
//

#ifndef RIGID_BODY_COMPONENT_H
#define RIGID_BODY_COMPONENT_H

#include <string_view>

#include <json/json.hpp>
#include <JoltPhysics/Jolt/Jolt.h>
#include <JoltPhysics/Jolt/Physics/Body/BodyID.h>


#include "engine/core/game_object/component_container.h"
#include "engine/core/game_object/components/component.h"
#include "engine/physics/physics_body.h"
#include "engine/physics/physics_types.h"

namespace will_engine
{
class ITransformable;
}

namespace will_engine::game
{
class RigidBodyComponent : public Component, public IPhysicsBody
{
public:
    explicit RigidBodyComponent(const std::string& name = "");

    ~RigidBodyComponent() override;

public:
    bool hasRigidBody() const { return bodyId.GetIndex() != JPH::BodyID::cMaxBodyIndex; }

private:
    ITransformable* transformableOwner{nullptr};

    JPH::BodyID bodyId{JPH::BodyID::cMaxBodyIndex};

    bool bIsPhysicsDirty{true};

    glm::vec3 offset{};

public:
    void releaseRigidBody();

    void beginDestroy() override;

    void setOwner(IComponentContainer* owner) override;

public: // IPhysicsBody
    void setTransform(const glm::vec3& position, const glm::quat& rotation) override;

    void dirty() override { bIsPhysicsDirty = true; }

    void undirty() override { bIsPhysicsDirty = false; }

    glm::vec3 getGlobalPosition() override;

    glm::quat getGlobalRotation() override;

    void setPhysicsBodyId(const JPH::BodyID bodyId) override { this->bodyId = bodyId; }

    JPH::BodyID getPhysicsBodyId() const override { return bodyId; }

    bool isTransformDirty() override { return bIsPhysicsDirty; }

public: // Serialization
    void serialize(ordered_json& j) override;

    void deserialize(ordered_json& j) override;

public: // Editor Tools
    void updateRenderImgui() override;

public:
    static constexpr auto TYPE = "RigidBodyComponent";
    static constexpr bool CAN_BE_CREATED_MANUALLY = true;

    static std::string_view getStaticType()
    {
        return TYPE;
    }

    std::string_view getComponentType() override { return TYPE; }
};
} // namespace will_engine::components

#endif // RIGID_BODY_COMPONENT_H
