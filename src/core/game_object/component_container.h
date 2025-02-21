//
// Created by William on 2025-02-21.
//

#ifndef COMPONENT_CONTAINER_H
#define COMPONENT_CONTAINER_H
#include <vector>
#include <string_view>
#include <typeinfo>

namespace will_engine
{
class Component;

class IComponentContainer
{
public:
    virtual ~IComponentContainer() = default;

    virtual Component* GetComponentByType(const std::type_info& type) = 0;

    virtual std::vector<Component*> GetComponentsByType(const std::type_info& type) = 0;

    template<typename T>
    T* GetComponent()
    {
        return static_cast<T*>(GetComponentByType(typeid(T)));
    }

    template<typename T>
    std::vector<T*> GetComponents()
    {
        std::vector<Component*> baseComponents = GetComponentsByType(typeid(T));
        std::vector<T*> typed;
        typed.reserve(baseComponents.size());
        for (auto* comp : baseComponents) {
            typed.push_back(static_cast<T*>(comp));
        }
        return typed;
    }

    virtual std::vector<Component*> getAllComponents() = 0;

    virtual void addComponent(Component* component) = 0;

    virtual void destroyComponent() = 0;

    [[nodiscard]] virtual std::string_view getName() const = 0;
};
}

#endif //COMPONENT_CONTAINER_H
