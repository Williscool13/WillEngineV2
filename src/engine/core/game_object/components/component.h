//
// Created by William on 2025-02-21.
//

#ifndef BASE_COMPONENT_H
#define BASE_COMPONENT_H

#include <string_view>
#include <json/json.hpp>

#include "engine/core/game_object/component_container.h"


namespace will_engine::components
{
using ordered_json = nlohmann::ordered_json;

class Component
{
public: // Virtuals
    Component() = delete;

    explicit Component(std::string name = "")
        : componentName(std::move(name)) {}

    virtual ~Component() = default;

    virtual void beginPlay();

    virtual void update(float deltaTime) {}

    virtual void beginDestroy();

    virtual void onEnable() {}

    virtual void onDisable() {}

    bool isComponentDestroyed() const { return bIsDestroyed; }

    IComponentContainer* getOwner() const { return owner; }

    virtual void setOwner(IComponentContainer* owner) { this->owner = owner; }

public: // Serialization / Editor Tools
    virtual void serialize(ordered_json& j) {}

    virtual void deserialize(ordered_json& j) {}

    virtual void openRenderImgui() {}
    virtual void updateRenderImgui() {}
    virtual void closeRenderImgui() {}

public: // Defined Behaviors
    void setComponentName(char objectName[256])
    {
        componentName = objectName;
    }

    std::string getComponentName()
    {
        return componentName;
    }

    std::string& getComponentNameRef()
    {
        return componentName;
    }

    std::string_view getComponentNameView()
    {
        if (componentName.empty()) {
            return getComponentType();
        }
        return componentName;
    }

    void enableComponent()
    {
        if (!bIsEnabled) {
            bIsEnabled = true;
            onEnable();
        }
    }

    void disableComponent()
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

public:
    virtual std::string_view getComponentType() = 0;
};
}


#endif //BASE_COMPONENT_H
