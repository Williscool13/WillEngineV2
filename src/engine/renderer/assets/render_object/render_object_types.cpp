//
// Created by William on 2025-01-24.
//
#include "render_object_types.h"

namespace will_engine
{
BoundingSphere::BoundingSphere(const std::vector<VertexPosition>& vertices)
{
    this->center = {0, 0, 0};

    for (auto&& vertex : vertices) {
        this->center += vertex.position;
    }
    this->center /= static_cast<float>(vertices.size());
    this->radius = glm::distance2(vertices[0].position, this->center);
    for (size_t i = 1; i < vertices.size(); ++i) {
        this->radius = std::max(this->radius, glm::distance2(vertices[i].position, this->center));
    }
    this->radius = std::nextafter(sqrtf(this->radius), std::numeric_limits<float>::max());
}

glm::vec4 BoundingSphere::getBounds(const std::vector<VertexPosition>& vertices)
{
    glm::vec3 center = {0, 0, 0};

    for (auto&& vertex : vertices) {
        center += vertex.position;
    }
    center /= static_cast<float>(vertices.size());
    float radius = glm::distance2(vertices[0].position, center);
    for (size_t i = 1; i < vertices.size(); ++i) {
        radius = std::max(radius, glm::distance2(vertices[i].position, center));
    }
    radius = std::nextafter(sqrtf(radius), std::numeric_limits<float>::max());

    return {radius, center};
}
}
