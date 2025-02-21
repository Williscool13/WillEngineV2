//
// Created by William on 2025-02-21.
//

#include "name_printing_component.h"

#include <imgui.h>

#include "fmt/format.h"
#include "src/core/game_object/component_container.h"

void will_engine::NamePrintingComponent::update(float deltaTime)
{
    fmt::print("Component Owner's name is: {}", owner->getName());
}

void will_engine::NamePrintingComponent::serialize(ordered_json& j)
{
    Component::serialize(j);
    j["testProperty"] = testProperty;
}

void will_engine::NamePrintingComponent::deserialize(ordered_json& j)
{
    Component::deserialize(j);
    testProperty = j["testProperty"].get<int32_t>();
}

void will_engine::NamePrintingComponent::selectedRenderImgui()
{
    Component::selectedRenderImgui();
    ImGui::Text(fmt::format("Class: {}", componentType).c_str());
    if (componentName.empty()) {
        ImGui::Text("Name: <empty>");
    } else {
        ImGui::Text(fmt::format("Name: {}", componentName).c_str());
    }
}
