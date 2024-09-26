//
// Created by William on 2024-09-26.
//

#ifndef BOUNDING_SPHERE_H
#define BOUNDING_SPHERE_H

#include "../vk_types.h"
#include "glm/vec3.hpp"

struct BoundingSphere {
    BoundingSphere() = default;
    explicit BoundingSphere(const std::vector<Vertex>& vertices);

    glm::vec3 center{};
    float radius{};
};


#endif //BOUNDING_SPHERE_H
