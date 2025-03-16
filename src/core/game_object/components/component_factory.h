//
// Created by William on 2025-02-21.
//

#ifndef COMPONENT_FACTORY_H
#define COMPONENT_FACTORY_H

#include "mesh_renderer_component.h"
#include "name_printing_component.h"
#include "rigid_body_component.h"
#include "terrain_component.h"
#include "src/core/game_object/components/component.h"

namespace will_engine::components
{
template<typename T>
concept HasGetStaticType = requires(T)
{
    { T::getStaticType() } -> std::convertible_to<std::string_view>;
};


class ComponentFactory
{
public:
    void registerComponents()
    {
        registerComponent<NamePrintingComponent>(NamePrintingComponent::CAN_BE_CREATED_MANUALLY);
        registerComponent<RigidBodyComponent>(RigidBodyComponent::CAN_BE_CREATED_MANUALLY);
        registerComponent<MeshRendererComponent>(MeshRendererComponent::CAN_BE_CREATED_MANUALLY);
        registerComponent<TerrainComponent>(TerrainComponent::CAN_BE_CREATED_MANUALLY);
    }

    using ComponentCreator = std::function<std::unique_ptr<Component>(const std::string& name)>;

    static ComponentFactory& getInstance()
    {
        static ComponentFactory instance;
        return instance;
    }

    const std::unordered_map<std::string_view, ComponentCreator>& getManuallyCreatableComponentCreators() { return canManuallyCreateCreators; }


    template<typename T>
    void registerComponent(bool canManuallyCreate) requires HasGetStaticType<T>
    {
        static_assert(std::is_base_of_v<Component, T>, "Must inherit from Component");
        allCreators[T::getStaticType()] = [](const std::string& name) { return std::make_unique<T>(name); };
        if (canManuallyCreate) {
            canManuallyCreateCreators[T::getStaticType()] = [](const std::string& name) { return std::make_unique<T>(name); };
        }
    }

    std::unique_ptr<Component> createComponent(const std::string_view& type, const std::string& name)
    {
        const auto it = allCreators.find(type);
        if (it != allCreators.end()) {
            return it->second(name);
        }
        return nullptr;
    }

private:
    ComponentFactory() = default;

    std::unordered_map<std::string_view, ComponentCreator> allCreators;

    std::unordered_map<std::string_view, ComponentCreator> canManuallyCreateCreators;
};
}
#endif //COMPONENT_FACTORY_H
