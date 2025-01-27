//
// Created by William on 2024-08-24.
//

#ifndef GAME_OBJECT_H
#define GAME_OBJECT_H

#include <string>

#include <glm/glm.hpp>

#include "renderable.h"
#include "transformable.h"
#include "src/core/transform.h"
#include "src/renderer/renderer_constants.h"
#include "src/renderer/render_object/render_reference.h"
#include "src/physics/physics.h"
#include "src/physics/physics_filters.h"
#include "src/physics/physics_utils.h"

namespace will_engine
{
class RenderObject;

class GameObject final : public IPhysicsBody, public IRenderable, public ITransformable
{
public:
    GameObject();

    explicit GameObject(std::string gameObjectName);

    ~GameObject() override;

    virtual void update(float deltaTime) {}

public: // Hierarchy
    /**
     * Adds a \code game_object\endcode as a child to this \code game_object\endcode while maintaining its position in WorldSpace.
     * \n Will reparent the child if that child has a parent.
     * @param child
     * @param maintainWorldTransform
     */
    void addChild(GameObject* child, bool maintainWorldTransform = true);

    /**
     * If exists, removes a child from the children array and unparent the child.
     */
    void removeChild(GameObject* child, GameObject* newParent = nullptr);

    /**
     * Unparents a child while maintaining its position in WorldSpace
     */
    void reparent(GameObject* newParent = nullptr, bool maintainWorldTransform = true);

    void setName(std::string newName);

    GameObject* getParent() const;

    std::vector<GameObject*>& getChildren();

    uint32_t getId() const { return gameObjectId; }

    std::string_view getName() const { return gameObjectName; }

protected: // Hierarchy
    GameObject* parent{nullptr};
    std::vector<GameObject*> children{};
    static uint32_t nextId;
    uint32_t gameObjectId{};
    std::string gameObjectName{};

public: // ITransformable
    glm::mat4 getModelMatrix() override { return getGlobalTransform().getTRSMatrix(); }

    const Transform& getLocalTransform() const override { return transform; }
    glm::vec3 getLocalPosition() const override { return getLocalTransform().getPosition(); }
    glm::quat getLocalRotation() const override { return getLocalTransform().getRotation(); }
    glm::vec3 getLocalScale() const override { return getLocalTransform().getScale(); }

    const Transform& getGlobalTransform() override;

    /**
     * Interface of both IPhysicsBody and ITransformable
     * @return
     */
    glm::vec3 getGlobalPosition() override { return getGlobalTransform().getPosition(); }
    /**
     * Interface of both IPhysicsBody and ITransformable
     * @return
     */
    glm::quat getGlobalRotation() override { return getGlobalTransform().getRotation(); }
    glm::vec3 getGlobalScale() override { return getGlobalTransform().getScale(); }

    void setLocalPosition(glm::vec3 localPosition) override;

    void setLocalRotation(glm::quat localRotation) override;

    void setLocalScale(glm::vec3 localScale) override;

    void setLocalScale(float localScale) override;

    void setLocalTransform(const Transform& newLocalTransform) override;

    void setGlobalPosition(glm::vec3 globalPosition) override;

    void setGlobalRotation(glm::quat globalRotation) override;

    void setGlobalScale(glm::vec3 newScale) override;

    void setGlobalScale(float globalScale) override;

    void setGlobalTransform(const Transform& newGlobalTransform) override;

    void translate(glm::vec3 translation) override;

    void rotate(glm::quat rotation) override;

    void rotateAxis(float angle, const glm::vec3& axis) override;

private: // Transform
    Transform transform{};
    Transform cachedGlobalTransform{};
    bool bIsGlobalTransformDirty{true};
    /**
     * Will update the model matrix on GPU if this number is greater than 0
     */
    int32_t framesToUpdate{FRAME_OVERLAP + 1};

    void dirty();

public: // IRenderable
    /**
     * Update the model transform in the RenderObject. Only applicable if this gameobject has a pRenderObject associated with it.
     */
    void recursiveUpdateModelMatrix(int32_t currentFrameOverlap, int32_t previousFrameOverlap);

    void setRenderObjectReference(IRenderReference* owner, const int32_t index) override
    {
        pRenderReference = owner;
        instanceIndex = index;
    }

private: // IRenderable
    /**
      * If true, the model matrix will never be updated from defaults.
      */
    bool bIsStatic{false};
    /**
     * The render object that is responsible for drawing this gameobject's model
     */
    IRenderReference* pRenderReference{nullptr};
    int32_t instanceIndex{-1};

public: // Physics
    void setGlobalTransformFromPhysics(const glm::vec3& position, const glm::quat& rotation) override;

    void setBodyId(const JPH::BodyID bodyId) override { this->bodyId = bodyId; }

    JPH::BodyID getBodyId() const override { return bodyId; }

    void setupRigidbody(const JPH::ShapeRefC& shape, const JPH::EMotionType motionType = JPH::EMotionType::Static, const JPH::ObjectLayer layer = physics::Layers::NON_MOVING);

private: // Physics
    JPH::BodyID bodyId{JPH::BodyID::cInvalidBodyID};

public:
    bool operator==(const GameObject& other) const
    {
        return this->gameObjectId == other.gameObjectId;
    }
};
}

#endif //GAME_OBJECT_H
