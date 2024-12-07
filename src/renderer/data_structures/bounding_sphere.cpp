//
// Created by William on 2024-09-26.
//

#include "bounding_sphere.h"

#include "glm/glm.hpp"
#include "glm/gtx/norm.hpp"

BoundingSphere::BoundingSphere(const std::vector<Vertex>& vertices)
{
    this->center = { 0, 0, 0 };

    for (auto&& vertex : vertices)
    {
        this->center += vertex.position;
    }
    this->center /= static_cast<float>(vertices.size());
    this->radius = glm::distance2(vertices[0].position, this->center);
    for (size_t i = 1; i < vertices.size(); ++i)
    {
        this->radius = std::max(this->radius, glm::distance2(vertices[i].position, this->center));
    }
    this->radius = std::nextafter(sqrtf(this->radius), std::numeric_limits<float>::max());
}
