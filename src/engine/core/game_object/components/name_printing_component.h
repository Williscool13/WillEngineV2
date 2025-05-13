//
// Created by William on 2025-02-21.
//

#ifndef NAME_PRINTING_COMPONENT_H
#define NAME_PRINTING_COMPONENT_H

#include "component.h"

namespace will_engine::components
{
class NamePrintingComponent final : public Component
{
public:
    NamePrintingComponent() = delete;

    explicit NamePrintingComponent(const std::string& name)
        : Component(name) {}

    static constexpr auto TYPE = "NamePrintingComponent";
    static constexpr bool CAN_BE_CREATED_MANUALLY = true;

    static std::string_view getStaticType()
    {
        return TYPE;
    }

    std::string_view getComponentType() override { return TYPE; }

    void beginPlay() override;

    void update(float deltaTime) override;

public: // Serialization

    void serialize(ordered_json& j) override;

    void deserialize(ordered_json& j) override;


public: // ImGui
    static char objectName[256];
    void openRenderImgui() override;

    void updateRenderImgui() override;

    void closeRenderImgui() override;

protected:
    int32_t testProperty{};
};
}

#endif //NAME_PRINTING_COMPONENT_H
