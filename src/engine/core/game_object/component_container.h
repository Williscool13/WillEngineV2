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

    virtual std::vector<components::Component*> getComponentsImpl(const std::type_info& type) = 0;

    template<typename T>
    T* getComponent()
    {
        static_assert(std::is_base_of_v<components::Component, T>, "T must inherit from Component");
        return static_cast<T*>(getComponentImpl(typeid(T)));
    }

    template<typename T>
    std::vector<T*> getComponents()
    {
        static_assert(std::is_base_of_v<components::Component, T>, "T must inherit from Component");
        std::vector<components::Component*> baseComponents = getComponentsImpl(typeid(T));
        std::vector<T*> typed;
        typed.reserve(baseComponents.size());
        for (auto* comp : baseComponents) {
            typed.push_back(static_cast<T*>(comp));
        }
        return typed;
    }

    template<typename T>
    void getComponentsInto(std::vector<T*>& outComponents)
    {
        static_assert(std::is_base_of_v<components::Component, T>, "T must inherit from Component");
        std::vector<components::Component*> baseComponents = getComponentsImpl(typeid(T));
        outComponents.clear();
        outComponents.reserve(baseComponents.size());
        for (auto* comp : baseComponents) {
            outComponents.push_back(static_cast<T*>(comp));
        }
    }

    virtual std::vector<components::Component*> getAllComponents() = 0;

    virtual components::Component* getComponentByTypeName(std::string_view componentType) = 0;

    virtual std::vector<components::Component*> getComponentsByTypeName(std::string_view componentType) = 0;

    virtual bool hasComponent(std::string_view componentType) = 0;

    virtual components::Component* addComponent(std::unique_ptr<components::Component> component) = 0;

    virtual void destroyComponent(components::Component* component) = 0;

    virtual components::RigidBodyComponent* getRigidbody() const = 0;

    [[nodiscard]] virtual std::string_view getName() const = 0;
};
}

#endif //COMPONENT_CONTAINER_H
