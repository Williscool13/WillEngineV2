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
        : intensity(intensity), color(color)
    {
        const glm::vec<3, float> normDirection = normalize(direction);
        this->direction[0] = normDirection.x;
        this->direction[1] = normDirection.y;
        this->direction[2] = normDirection.z;
    }

    ~DirectionalLight() = default;

    [[nodiscard]] glm::vec3 getDirection() const { return glm::vec3(direction[0], direction[1], direction[2]); }
    [[nodiscard]] float getIntensity() const { return intensity; }
    [[nodiscard]] glm::vec3 getColor() const { return color; }

    DirectionalLightData getData() const { return {glm::vec3(direction[0], direction[1], direction[2]), intensity, color}; }

    friend void ImguiWrapper::imguiInterface(Engine* engine);

private:
    float direction[3]{};
    float intensity{};
    glm::vec3 color{};
};
}


#endif //DIRECTIONAL_LIGHT_H
