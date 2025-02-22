//
// Created by William on 2025-02-22.
//

#ifndef RIGID_BODY_COMPONENT_H
#define RIGID_BODY_COMPONENT_H

#include <string_view>
#include <json/json.hpp>
#include "src/core/game_object/component_container.h"
#include "src/core/game_object/components/component.h"

namespace will_engine::components
{
class RigidBodyComponent : public Component
{
public:
    explicit RigidBodyComponent(const std::string& name = "");

    ~RigidBodyComponent() override;

    static constexpr auto TYPE = "RigidBodyComponent";

    static std::string_view getStaticType()
    {
        return TYPE;
    }

    std::string_view getComponentType() override { return TYPE; }

    void beginPlay(IComponentContainer* owner) override;

    void update(float deltaTime) override;

    void beginDestroy() override;

    void onEnable() override;

    void onDisable() override;

public: // Serialization
    void serialize(ordered_json& j) override;

    void deserialize(ordered_json& j) override;

public: // Editor Tools
    void openRenderImgui() override;

    void updateRenderImgui() override;

    void closeRenderImgui() override;

private:
    // Add component-specific members here
};
} // namespace will_engine::components

#endif // RIGID_BODY_COMPONENT_H
