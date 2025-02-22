//
// Created by William on 2025-02-22.
//

#include "rigid_body_component.h"

namespace will_engine::components
{
RigidBodyComponent::RigidBodyComponent(const std::string& name)
    : Component(name)
{}

RigidBodyComponent::~RigidBodyComponent() = default;

void RigidBodyComponent::beginPlay(IComponentContainer* owner)
{
    Component::beginPlay(owner);
    // Add initialization code here
}

void RigidBodyComponent::update(const float deltaTime)
{
    Component::update(deltaTime);
    // Add update logic here
}

void RigidBodyComponent::beginDestroy()
{
    Component::beginDestroy();
    // Add cleanup code here
}

void RigidBodyComponent::onEnable()
{
    Component::onEnable();
    // Add enable logic here
}

void RigidBodyComponent::onDisable()
{
    Component::onDisable();
    // Add disable logic here
}

void RigidBodyComponent::serialize(ordered_json& j)
{
    // Add serialization code here
}

void RigidBodyComponent::deserialize(ordered_json& j)
{
    // Add deserialization code here
}

void RigidBodyComponent::openRenderImgui()
{
    // Add ImGui setup code here
}

void RigidBodyComponent::updateRenderImgui()
{
    // Add ImGui update code here
}

void RigidBodyComponent::closeRenderImgui()
{
    // Add ImGui cleanup code here
}
} // namespace will_engine::components
