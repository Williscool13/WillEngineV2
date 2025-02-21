//
// Created by William on 2025-02-21.
//

#include "name_printing_component.h"

#include <imgui.h>

#include "fmt/format.h"
#include "src/core/game_object/component_container.h"


void will_engine::components::NamePrintingComponent::update(float deltaTime)
{
    fmt::print("Component Owner's name is: {}", owner->getName());
}

void will_engine::components::NamePrintingComponent::serialize(ordered_json& j)
{
    Component::serialize(j);
    j["testProperty"] = testProperty;
}

void will_engine::components::NamePrintingComponent::deserialize(ordered_json& j)
{
    Component::deserialize(j);
    testProperty = j["testProperty"].get<int32_t>();
}

void will_engine::components::NamePrintingComponent::selectedRenderImgui()
{
    Component::selectedRenderImgui();
    ImGui::Text(fmt::format("Class: {}", TYPE).c_str());
    if (componentName.empty()) {
        ImGui::Text("Name: <empty>");
    } else {
        ImGui::Text(fmt::format("Name: {}", componentName).c_str());
    }
}
