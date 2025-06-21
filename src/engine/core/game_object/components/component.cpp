//
// Created by William on 2025-02-21.
//

#include "component.h"


namespace will_engine::game
{
void Component::beginPlay()
{
    bHasBegunPlay = true;
}

void Component::beginDestroy()
{
    if (bIsDestroyed) {
        return;
    }

    bIsDestroyed = true;
    owner = nullptr;
}
}
