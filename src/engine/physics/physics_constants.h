//
// Created by William on 2025-01-23.
//

#ifndef PHYSICS_CONSTANTS_H
#define PHYSICS_CONSTANTS_H

namespace will_engine::physics
{
static constexpr uint32_t MAX_BODIES = 10240;
static constexpr uint32_t MAX_BODY_PAIRS = 10240;
static constexpr uint32_t MAX_CONTACT_CONSTRAINTS = 10240;
static constexpr uint32_t NUM_BODY_MUTEXES = 0;
static constexpr uint32_t TEMP_ALLOCATOR_SIZE = 10 * 1024 * 1024; // 10MB

static constexpr int32_t WORLD_FLOOR_HEIGHT = -20;

static constexpr uint32_t BODY_ID_NONE = 0xffffffff;

static constexpr auto UNIT_CUBE = glm::vec3(0.5f);
static constexpr auto UNIT_SPHERE = glm::vec3(1.0f, 0.0f, 0.0f);
static constexpr auto UNIT_CAPSULE = glm::vec3(1.0f, 0.5f, 0.0f);
static constexpr auto UNIT_CYLINDER = glm::vec3(1.0f,0.5f, 0.0f);
}
#endif //PHYSICS_CONSTANTS_H
