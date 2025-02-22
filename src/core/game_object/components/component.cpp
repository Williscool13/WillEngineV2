//
// Created by William on 2025-02-21.
//

#include "component.h"

void will_engine::components::Component::beginPlay(IComponentContainer* owner)
{
    bHasBegunPlay = true;
    this->owner = owner;;
}

void will_engine::components::Component::update(float deltaTime) {}

void will_engine::components::Component::beginDestroy()
{
    bIsDestroyed = true;
    owner = nullptr;
}

void will_engine::components::Component::onEnable() {}
void will_engine::components::Component::onDisable() {}
