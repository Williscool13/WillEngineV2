//
// Created by William on 2025-02-21.
//

#include "name_printing_component.h"

#include <imgui.h>

#include "fmt/format.h"
#include "src/core/game_object/component_container.h"


char will_engine::components::NamePrintingComponent::objectName[256] = "";

void will_engine::components::NamePrintingComponent::beginPlay(IComponentContainer* owner)
{
    Component::beginPlay(owner);

    fmt::print("Hello World! (Component)\n");
}

void will_engine::components::NamePrintingComponent::update(float deltaTime)
{

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

void will_engine::components::NamePrintingComponent::openRenderImgui()
{
    Component::openRenderImgui();
    strncpy_s(objectName, sizeof(objectName), componentName.c_str(), _TRUNCATE);
}

void will_engine::components::NamePrintingComponent::updateRenderImgui()
{
    Component::updateRenderImgui();

    ImGui::Text(fmt::format("Class: {}", TYPE).c_str());
    if (ImGui::InputText("Component Name", objectName, sizeof(objectName))) {
        componentName = objectName;
    }

    ImGui::InputInt("Test Property", &testProperty);


    ImGui::Separator();

    ImGui::TextDisabled("Current Values:");
    ImGui::Text("Name: %s", componentName.empty() ? "<empty>" : componentName.c_str());
    ImGui::Text("Test Property: %d", testProperty);
}

void will_engine::components::NamePrintingComponent::closeRenderImgui()
{
    Component::closeRenderImgui();
}
