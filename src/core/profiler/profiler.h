//
// Created by William on 2025-02-16.
//

#ifndef PROFILER_H
#define PROFILER_H
#include <map>

#include "src/util/profiling_utils.h"

namespace will_engine
{
struct StartupProfilerEntry
{
    std::string name;
    std::chrono::steady_clock::time_point time{};
};
}

class StartupProfiler
{
public:
    void addEntry(const std::string& name)
    {
        entries.push_back({std::move(name), std::chrono::steady_clock::now()});
    }

    const std::vector<will_engine::StartupProfilerEntry>& getEntries()
    {
        return entries;
    }


private:
    std::vector<will_engine::StartupProfilerEntry> entries;
};

class Profiler
{
public:
    void addTimer(const std::string& name)
    {
        if (!timers.contains(name)) {
            timers.emplace(std::move(name), ProfilingData{});
        }
    }

    void beginTimer(const std::string& name)
    {
        if (const auto it = timers.find(name); it != timers.end()) {
            it->second.begin();
        }
    }

    void endTimer(const std::string& name)
    {
        if (const auto it = timers.find(name); it != timers.end()) {
            it->second.end();
        }
    }

    [[nodiscard]] const auto& getTimers() const { return timers; }


private:
    std::map<std::string, ProfilingData> timers;
};


#endif //PROFILER_H
