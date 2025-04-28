//
// Created by William on 2024-08-24.
//

#include "time.h"

#include <SDL_timer.h>

namespace will_engine
{
Time::Time()
{
    lastTime = SDL_GetTicks();
}


void Time::update()
{
    const uint64_t last = SDL_GetTicks();
    deltaTime = last - lastTime;
    // Breakpoint resume case
    if (deltaTime > 1000) { deltaTime = 333; }
    lastTime = last;

}

float Time::getDeltaTime() const
{
    return static_cast<float>(deltaTime) / 1000.0f;
}

float Time::getTime() const
{
    return static_cast<float>(lastTime) / 1000.0f;
}
}
