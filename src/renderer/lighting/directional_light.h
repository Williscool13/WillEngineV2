//
// Created by William on 2025-01-01.
//

#ifndef DIRECTIONAL_LIGHT_H
#define DIRECTIONAL_LIGHT_H

#include <glm/glm.hpp>

#include "src/renderer/imgui_wrapper.h"

namespace will_engine
{
struct DirectionalLightData
{
    glm::vec3 direction;
    float intensity;
    glm::vec3 color;
    float pad;
};

class DirectionalLight
{
public:
    DirectionalLight() = delete;

    DirectionalLight(const glm::vec3 direction, const float intensity, const glm::vec3 color = {1.0f, 1.0f, 1.0f})
        : direction(normalize(direction)), intensity(intensity), color(color)
    {}

    ~DirectionalLight() = default;

    [[nodiscard]] glm::vec3 getDirection() const { return direction; }
    [[nodiscard]] float getIntensity() const { return intensity; }
    [[nodiscard]] glm::vec3 getColor() const { return color; }

    DirectionalLightData getData() const { return {direction, intensity, color}; }

    friend void ImguiWrapper::imguiInterface(Engine* engine);

private:
    glm::vec3 direction{};
    float intensity{};
    glm::vec3 color{};
};
}


#endif //DIRECTIONAL_LIGHT_H
