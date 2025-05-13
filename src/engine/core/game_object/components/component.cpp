//
// Created by William on 2025-02-21.
//

#include "component.h"

void will_engine::components::Component::beginPlay()
{
    bHasBegunPlay = true;
}

void will_engine::components::Component::beginDestroy()
{
    if (bIsDestroyed) {
        return;
    }

    bIsDestroyed = true;
    owner = nullptr;
}
