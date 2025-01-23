//
// Created by William on 2024-08-24.
//

#ifndef TIME_UTILS_H
#define TIME_UTILS_H
#include <SDL.h>


namespace will_engine
{
class Time
{
public:
    static Time& Get()
    {
        static Time instance{};
        return instance;
    }


    Time();

    void update();

    [[nodiscard]] float getDeltaTime() const;

private:
    uint64_t deltaTime = 0;
    uint64_t lastTime = 0;
};
}


#endif //TIME_UTILS_H
