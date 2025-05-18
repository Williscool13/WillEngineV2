//
// Created by William on 2024-08-24.
//

#include "transform.h"

namespace will_engine
{
const Transform Transform::Identity = Transform(
    glm::vec3(0.0f),
    glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
    glm::vec3(1.0f)
);
}
