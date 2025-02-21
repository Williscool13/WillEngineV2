//
// Created by William on 2025-02-21.
//

#include "name_printing_component.h"
#include "fmt/format.h"
#include "src/core/game_object/component_container.h"

void will_engine::NamePrintingComponent::update(float deltaTime)
{
    fmt::print("Component Owner's name is: {}", owner->getName());
}