//
// Created by William on 2025-02-21.
//

#include "component.h"

void will_engine::Component::beginPlay(IComponentContainer* owner)
{
    bHasBegunPlay = true;
}

void will_engine::Component::update(float deltaTime) {}

void will_engine::Component::beginDestroy()
{
    bIsDestroyed = true;
    owner = nullptr;
}

void will_engine::Component::onEnable() {}
void will_engine::Component::onDisable() {}
