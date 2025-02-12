//
// Created by William on 2024-08-24.
//

#ifndef GAME_OBJECT_H
#define GAME_OBJECT_H

#include <string>
#include <extern/half/half/half.hpp>

#include <glm/glm.hpp>

#include "hierarchical.h"
#include "renderable.h"
#include "transformable.h"
#include "src/core/transform.h"
#include "src/physics/physics_body.h"
#include "src/physics/physics_constants.h"
#include "src/physics/physics_utils.h"
#include "src/renderer/renderer_constants.h"
#include "src/renderer/render_object/render_reference.h"
#include "src/util/math_constants.h"

namespace will_engine
{
class Engine;
class RenderObject;

class GameObject : public IPhysicsBody, public IRenderable, public ITransformable, public IHierarchical, public IIdentifiable
{
public:
    explicit GameObject(std::string gameObjectName = "", uint64_t gameObjectId = INDEX64_NONE);

    ~GameObject() override;

    virtual void update(float deltaTime) {}

public: // IIdentifiable
    void setId(const uint64_t identifier) override { gameObjectId = identifier; }

    uint64_t getId() const override { return gameObjectId; }

private: // IIdentifiable
    uint64_t gameObjectId{};

public: // IHierarchical
    bool addChild(IHierarchical* child) override;

    bool removeChild(IHierarchical* child) override;

    void reparent(IHierarchical* newParent) override;

    void dirty() override;

    void recursiveUpdate(int32_t currentFrameOverlap, int32_t previousFrameOverlap) override;

    void setParent(IHierarchical* newParent) override;

    IHierarchical* getParent() const override;

    const std::vector<IHierarchical*>& getChildren() const override;

    std::vector<IHierarchical*>& getChildren() override;

    void setName(std::string newName) override;

    std::string_view getName() const override { return gameObjectName; }

protected: // IHierarchical
    IHierarchical* parent{nullptr};
    /**
     * Cache parent's transformable interface to reduce excessive casting
     */
    ITransformable* transformableParent{nullptr};
    std::vector<IHierarchical*> children{};
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

protected: // Transform
    Transform transform{};
    Transform cachedGlobalTransform{};
    bool bIsGlobalTransformDirty{true};
    /**
     * Will update the model matrix on GPU if this number is greater than 0
     */
    int32_t framesToUpdate{FRAME_OVERLAP + 1};

public: // IRenderable
    void setRenderObjectReference(IRenderReference* owner, const int32_t instanceIndex, const int32_t meshIndex) override
    {
        pRenderReference = owner;
        this->instanceIndex = instanceIndex;
        framesToUpdate = FRAME_OVERLAP + 1;
        this->meshIndex = meshIndex;
    }

    uint32_t getRenderReferenceIndex() const override { return pRenderReference ? pRenderReference->getId() : INDEX_NONE; }

    int32_t getMeshIndex() const override { return meshIndex; }

    [[nodiscard]] bool& isVisible() override { return bIsVisible; }

    [[nodiscard]] bool& isCastingShadows() override { return bCastsShadows; }

protected: // IRenderable
    /**
      * If true, the model matrix will never be updated from defaults.
      */
    bool bIsStatic{false};
    bool bIsVisible{true};
    bool bCastsShadows{false};
    /**
     * The render object that is responsible for drawing this gameobject's model
     */
    IRenderReference* pRenderReference{nullptr};
    int32_t instanceIndex{INDEX_NONE};
    int32_t meshIndex{INDEX_NONE};

public: // IPhysicsBody
    void setGlobalTransformFromPhysics(const glm::vec3& position, const glm::quat& rotation) override;

    void setPhysicsBodyId(const JPH::BodyID bodyId) override { this->bodyId = bodyId; }

    JPH::BodyID getPhysicsBodyId() const override { return bodyId; }

protected: // IPhysicsBody
    JPH::BodyID bodyId{JPH::BodyID::cMaxBodyIndex};

public:
    bool operator==(const GameObject& other) const
    {
        return this->gameObjectId == other.gameObjectId;
    }
};
}

#endif //GAME_OBJECT_H
