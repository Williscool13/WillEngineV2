//
// Created by William on 2024-08-24.
//

#ifndef TIME_UTILS_H
#define TIME_UTILS_H
#include <SDL.h>


class TimeUtils
{
public:
    static TimeUtils &Get()
    {
        static TimeUtils instance{};
        return instance;
    }


    TimeUtils();

    void update();

    float getDeltaTime();

private:
    uint64_t deltaTime = 0;
    uint64_t lastTime = 0;
};


#endif //TIME_UTILS_H
