//
// Created by William on 2025-02-21.
//

#ifndef BASE_COMPONENT_H
#define BASE_COMPONENT_H
#include <json/json.hpp>

#include "src/core/game_object/component_container.h"


namespace will_engine
{
using ordered_json = nlohmann::ordered_json;

class Component
{
public: // Virtuals
    virtual ~Component() = default;

    virtual void beginPlay(IComponentContainer* owner);

    virtual void update(float deltaTime);

    virtual void beginDestroy();

    virtual void onEnable();

    virtual void onDisable();

    virtual IComponentContainer* getOwner() { return owner; }

    virtual bool isComponentDestroyed() { return bIsDestroyed; }

    virtual void serialize(ordered_json& j) const
    {
        j["componentName"] = componentName;
    }

    virtual void deserialize(ordered_json& j)
    {
        if (j.contains("componentName")) {
            componentName = j["componentName"].get<std::string>();
        }
    }


public: // Defined Behaviors
    void EnableComponent()
    {
        if (!bIsEnabled) {
            bIsEnabled = true;
            onEnable();
        }
    }

    void DisableComponent()
    {
        if (bIsEnabled) {
            bIsEnabled = false;
            onDisable();
        }
    }


    bool isEnabled() const { return bIsEnabled; }

protected:
    std::string componentName;

    bool bHasBegunPlay{false};
    bool bIsEnabled{true};
    bool bIsDestroyed{false};

    IComponentContainer* owner{};
};
}


#endif //BASE_COMPONENT_H
