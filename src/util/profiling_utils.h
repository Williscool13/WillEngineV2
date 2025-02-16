//
// Created by William on 2025-02-16.
//

#ifndef PROFILING_UTILS_H
#define PROFILING_UTILS_H
#include <chrono>

struct ProfilingData
{
    static constexpr float INITIAL_SAMPLE_COUNT = 100.0f;
    static constexpr int32_t INITIAL_SAMPLE_COUNT_INT = 100;
    static constexpr int32_t FINAL_SAMPLE_COUNT = 1;

    void begin()
    {
        start = std::chrono::steady_clock::now();
    }

    void end()
    {
        const auto now = std::chrono::steady_clock::now();
        const float elapsedTime = static_cast<float>(
                                      std::chrono::duration_cast<std::chrono::microseconds>(now - start).count()
                                  ) / 1000.0f;

        const float alpha = sampleCount / INITIAL_SAMPLE_COUNT;
        accumulatedTime = accumulatedTime * (1 - alpha) + elapsedTime * alpha;
        if (sampleCount > FINAL_SAMPLE_COUNT) {
            sampleCount--;
        }
    }

    [[nodiscard]] float getAverageTime() const { return accumulatedTime; }

private:
    float accumulatedTime{0.0f};
    std::chrono::steady_clock::time_point start{};
    int32_t sampleCount{INITIAL_SAMPLE_COUNT_INT};
};

#endif //PROFILING_UTILS_H
