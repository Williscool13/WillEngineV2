//
// Created by William on 2025-02-21.
//

#ifndef BASE_COMPONENT_H
#define BASE_COMPONENT_H

#include <string_view>
#include <json/json.hpp>

#include "src/core/game_object/component_container.h"


namespace will_engine::components
{
using ordered_json = nlohmann::ordered_json;

class Component
{
public: // Virtuals
    Component() = delete;

    explicit Component(const std::string name = "")
        : componentName(std::move(name)) {}

    virtual ~Component() = default;

    virtual std::string_view getComponentType() = 0;

    virtual void beginPlay(IComponentContainer* owner);

    virtual void update(float deltaTime);

    virtual void beginDestroy();

    virtual void onEnable();

    virtual void onDisable();

    virtual IComponentContainer* getOwner() { return owner; }

    virtual bool isComponentDestroyed() { return bIsDestroyed; }

public: // Serialization / Editor Tools
    virtual void serialize(ordered_json& j) {}

    virtual void deserialize(ordered_json& j) {}

    virtual void selectedRenderImgui() {}

    std::string_view getComponentName()
    {
        if (componentName.empty()) {
            return getComponentType();
        }
        return componentName;
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
