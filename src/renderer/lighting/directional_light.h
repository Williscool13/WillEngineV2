//
// Created by William on 2025-01-01.
//

#ifndef DIRECTIONAL_LIGHT_H
#define DIRECTIONAL_LIGHT_H

#include <glm/glm.hpp>

class DirectionalLight
{
public:
    DirectionalLight() = delete;

    DirectionalLight(glm::vec3 direction, float intensity, glm::vec3 color = {1.0f, 1.0f, 1.0f})
        : direction(direction), intensity(intensity), color(color) {}

    ~DirectionalLight() = default;

private:
    glm::vec3 direction{};
    float intensity{};
    glm::vec3 color{};
};


#endif //DIRECTIONAL_LIGHT_H
