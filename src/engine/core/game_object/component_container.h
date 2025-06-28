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
namespace game
{
    class Component;
}

class IComponentContainer
{
public:
    virtual ~IComponentContainer() = default;

    virtual game::Component* getComponentImpl(const std::type_info& type) = 0;

    virtual std::vector<game::Component*> getComponentsImpl(const std::type_info& type) = 0;

    template<typename T>
    T* getComponent()
    {
        static_assert(std::is_base_of_v<game::Component, T>, "T must inherit from Component");
        return static_cast<T*>(getComponentImpl(typeid(T)));
    }

    template<typename T>
    std::vector<T*> getComponents()
    {
        static_assert(std::is_base_of_v<game::Component, T>, "T must inherit from Component");
        std::vector<game::Component*> baseComponents = getComponentsImpl(typeid(T));
        std::vector<T*> typed;
        typed.reserve(baseComponents.size());
        for (auto* comp : baseComponents) {
            typed.push_back(static_cast<T*>(comp));
        }
        return typed;
    }

    template<typename Interface>
    std::vector<Interface*> getComponentsImplementing()
    {
        static_assert(std::is_abstract_v<Interface> || std::is_class_v<Interface>,
                      "Interface must be a class or interface type");

        std::vector<game::Component*> allComponents = getAllComponents(); // or however you get all components
        std::vector<Interface*> implementing;

        for (auto* comp : allComponents) {
            if (auto* interface = dynamic_cast<Interface*>(comp)) {
                implementing.push_back(interface);
            }
        }

        return implementing;
    }

    template<typename T>
    void getComponentsInto(std::vector<T*>& outComponents)
    {
        static_assert(std::is_base_of_v<game::Component, T>, "T must inherit from Component");
        std::vector<game::Component*> baseComponents = getComponentsImpl(typeid(T));
        outComponents.clear();
        outComponents.reserve(baseComponents.size());
        for (auto* comp : baseComponents) {
            outComponents.push_back(static_cast<T*>(comp));
        }
    }

    virtual std::vector<game::Component*> getAllComponents() = 0;

    virtual game::Component* getComponentByTypeName(std::string_view componentType) = 0;

    virtual std::vector<game::Component*> getComponentsByTypeName(std::string_view componentType) = 0;

    virtual bool hasComponent(std::string_view componentType) = 0;

    virtual game::Component* addComponent(std::unique_ptr<game::Component> component) = 0;

    virtual void destroyComponent(game::Component* component) = 0;

    [[nodiscard]] virtual std::string_view getName() const = 0;
};
}

#endif //COMPONENT_CONTAINER_H
