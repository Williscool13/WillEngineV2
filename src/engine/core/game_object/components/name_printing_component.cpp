//
// Created by William on 2025-02-21.
//

#include "name_printing_component.h"

#include <imgui.h>

#include "fmt/format.h"
#include "engine/core/game_object/component_container.h"

namespace will_engine::game
{
char NamePrintingComponent::objectName[256] = "";

void NamePrintingComponent::beginPlay()
{
    Component::beginPlay();

    fmt::print("Hello World! (Component)\n");
}

void NamePrintingComponent::update(float deltaTime)
{}

void NamePrintingComponent::serialize(ordered_json& j)
{
    Component::serialize(j);
    j["testProperty"] = testProperty;
}

void NamePrintingComponent::deserialize(ordered_json& j)
{
    Component::deserialize(j);
    testProperty = j["testProperty"].get<int32_t>();
}

void NamePrintingComponent::openRenderImgui()
{
    Component::openRenderImgui();
    strncpy_s(objectName, sizeof(objectName), componentName.c_str(), _TRUNCATE);
}

void NamePrintingComponent::updateRenderImgui()
{
    Component::updateRenderImgui();

    ImGui::InputInt("Test Property", &testProperty);
}

void NamePrintingComponent::closeRenderImgui()
{
    Component::closeRenderImgui();
}
}
