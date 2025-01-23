//
// Created by William on 2024-08-24.
//

#include "time.h"

namespace will_engine
{
Time::Time()
{
    lastTime = SDL_GetTicks64();
}


void Time::update()
{
    const uint64_t last = SDL_GetTicks64();
    deltaTime = last - lastTime;
    // Breakpoint resume case
    if (deltaTime > 1000) { deltaTime = 333; }
    lastTime = last;
}

float Time::getDeltaTime() const
{
    return static_cast<float>(deltaTime) / 1000.0f;
}
}
