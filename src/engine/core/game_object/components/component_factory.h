//
// Created by William on 2025-02-21.
//

#ifndef COMPONENT_FACTORY_H
#define COMPONENT_FACTORY_H

#include "mesh_renderer_component.h"
#include "name_printing_component.h"
#include "rigid_body_component.h"
#include "terrain_component.h"
#include "engine/core/factory/object_factory.h"
#include "engine/core/game_object/components/component.h"

namespace will_engine::game
{
class ComponentFactory : public ObjectFactory<Component>
{
public:
    static ComponentFactory& getInstance()
    {
        static ComponentFactory instance;
        return instance;
    }

    void registerComponents()
    {
        registerType<NamePrintingComponent>(NamePrintingComponent::CAN_BE_CREATED_MANUALLY);
        registerType<RigidBodyComponent>(RigidBodyComponent::CAN_BE_CREATED_MANUALLY);
        registerType<MeshRendererComponent>(MeshRendererComponent::CAN_BE_CREATED_MANUALLY);
        registerType<TerrainComponent>(TerrainComponent::CAN_BE_CREATED_MANUALLY);
    }
};
}
#endif //COMPONENT_FACTORY_H
