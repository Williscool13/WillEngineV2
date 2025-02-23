//
// Created by William on 2025-02-21.
//

#include "component.h"

#include <fmt/format.h>

will_engine::components::Component::~Component()
{
    fmt::print("Destroying Component\n");
}

void will_engine::components::Component::beginPlay()
{
    bHasBegunPlay = true;
}

void will_engine::components::Component::update(float deltaTime) {}

void will_engine::components::Component::beginDestroy()
{
    if (bIsDestroyed) {
        return;
    }

    bIsDestroyed = true;
    owner = nullptr;
}

void will_engine::components::Component::onEnable() {}
void will_engine::components::Component::onDisable() {}
