//
// Created by William on 2025-02-21.
//

#ifndef NAME_PRINTING_COMPONENT_H
#define NAME_PRINTING_COMPONENT_H

#include "component.h"

namespace will_engine
{
class NamePrintingComponent final : public Component
{
public:
    NamePrintingComponent() = delete;

    explicit NamePrintingComponent(const std::string& name)
        : Component(name) {}

    std::string_view getComponentType() override { return componentType; }

    void update(float deltaTime) override;

    void serialize(ordered_json& j) override;

    void deserialize(ordered_json& j) override;

    void selectedRenderImgui() override;

protected:
    int32_t testProperty{};

    std::string componentType{"NamePrintingComponent"};
};
}

#endif //NAME_PRINTING_COMPONENT_H
