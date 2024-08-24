//
// Created by William on 2024-08-24.
//

#include "time_utils.h"

TimeUtils::TimeUtils()
{
    lastTime = SDL_GetTicks64();
}

void TimeUtils::update()
{
    const uint64_t last = SDL_GetTicks64();
    deltaTime = last - lastTime;
    lastTime = last;
}

float TimeUtils::getDeltaTime() const
{
    return static_cast<float>(deltaTime) / 1000.0f;
}
