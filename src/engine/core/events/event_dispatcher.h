//
// Created by William on 2025-06-19.
//

#ifndef EVENT_DISPATCHER_H
#define EVENT_DISPATCHER_H
#include <cstdint>
#include <vector>
#include <functional>

namespace will_engine
{
template<typename EventType>
class EventDispatcher
{
public:
    using Handle = uint32_t;
    static constexpr Handle INVALID_HANDLE = 0;

private:
    struct CallbackEntry
    {
        std::function<void(const EventType&)> callback;
        Handle handle{INVALID_HANDLE};
        bool active = true;
    };

    std::vector<CallbackEntry> callbacks;
    Handle nextHandle = 1;
    bool needsCleanup = false;

public:
    Handle subscribe(std::function<void(const EventType&)> callback)
    {
        Handle handle = nextHandle++;
        callbacks.push_back({std::move(callback), handle, true});
        return handle;
    }

    void unsubscribe(Handle handle)
    {
        for (auto& entry : callbacks) {
            if (entry.handle == handle && entry.active) {
                entry.active = false;
                needsCleanup = true;
                return;
            }
        }
    }

    void dispatch(const EventType& event)
    {
        if (needsCleanup) {
            cleanup();
        }

        for (const auto& entry : callbacks) {
            if (entry.active) {
                entry.callback(event);
            }
        }
    }

private:
    void cleanup()
    {
        callbacks.erase(
            std::remove_if(callbacks.begin(), callbacks.end(),
                           [](const CallbackEntry& entry) { return !entry.active; }),
            callbacks.end()
        );
        needsCleanup = false;
    }
};
} // will_engine

#endif //EVENT_DISPATCHER_H
