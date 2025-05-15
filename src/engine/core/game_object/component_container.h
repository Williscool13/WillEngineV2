//
// Created by William on 2025-02-21.
//

#ifndef COMPONENT_CONTAINER_H
#define COMPONENT_CONTAINER_H
#include <vector>
#include <string_view>
#include <memory>
#include <typeinfo>

namespace will_engine
{
namespace components
{
    class MeshRendererComponent;
    class RigidBodyComponent;
    class Component;
}

class IComponentContainer
{
public:
    virtual ~IComponentContainer() = default;

    virtual components::Component* getComponentImpl(const std::type_info& type) = 0;

    // todo: restore "GetComponents" from previously deleted commit.
    template<typename T>
    T* getComponent()
    {
        static_assert(std::is_base_of_v<components::Component, T>, "T must inherit from Component");
        return static_cast<T*>(getComponentImpl(typeid(T)));
    }

    virtual std::vector<components::Component*> getAllComponents() = 0;

    virtual components::Component* getComponentByTypeName(std::string_view componentType) = 0;

    virtual bool hasComponent(std::string_view componentType) = 0;

    virtual bool canAddComponent(std::string_view componentType) = 0;

    virtual components::Component* addComponent(std::unique_ptr<components::Component> component) = 0;

    virtual void destroyComponent(components::Component* component) = 0;

    virtual components::RigidBodyComponent* getRigidbody() const = 0;

    virtual components::MeshRendererComponent* getMeshRenderer() const = 0;

    [[nodiscard]] virtual std::string_view getName() const = 0;
};
}

#endif //COMPONENT_CONTAINER_H
