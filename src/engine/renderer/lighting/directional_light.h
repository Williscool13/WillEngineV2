//
// Created by William on 2025-01-01.
//

#ifndef DIRECTIONAL_LIGHT_H
#define DIRECTIONAL_LIGHT_H

#include <glm/glm.hpp>

namespace will_engine
{
struct DirectionalLightData
{
    glm::vec3 direction;
    float intensity;
    glm::vec3 color;
    float pad;
};

struct DirectionalLight
{
public:
    DirectionalLight() = delete;

    DirectionalLight(const glm::vec3 direction, const float intensity, const glm::vec3 color = {1.0f, 1.0f, 1.0f})
        : direction(normalize(direction)), intensity(intensity), color(color)
    {}

    DirectionalLightData getData() const { return {normalize(direction), intensity, color}; }

    glm::vec3 direction{};
    float intensity{};
    glm::vec3 color{};
};
}


#endif //DIRECTIONAL_LIGHT_H
