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
    void update(float deltaTime) override;
};
}

#endif //NAME_PRINTING_COMPONENT_H
