//
// Created by William on 8/11/2024.
//

#ifndef VKTYPES_H
#define VKTYPES_H

#include <glm/glm.hpp>

#include "lighting/directional_light.h"
namespace will_engine
{
struct SceneData
{
    glm::mat4 view{1.0f};
    glm::mat4 proj{1.0f};
    glm::mat4 viewProj{1.0f};

    glm::mat4 invView{1.0f};
    glm::mat4 invProj{1.0f};
    glm::mat4 invViewProj{1.0f};

    glm::mat4 viewProjCameraLookDirection{1.0f};

    glm::mat4 prevView{1.0f};
    glm::mat4 prevProj{1.0f};
    glm::mat4 prevViewProj{1.0f};

    glm::mat4 prevInvView{1.0f};
    glm::mat4 prevInvProj{1.0f};
    glm::mat4 prevInvViewProj{1.0f};

    glm::mat4 prevViewProjCameraLookDirection{1.0f};

    glm::vec4 cameraWorldPos{0.0f};
    glm::vec4 prevCameraWorldPos{0.0f};

    /**
     * x,y is current; z,w is previous
     */
    glm::vec4 jitter{0.0f};

    // vec4 + vec4
    DirectionalLightData mainLightData{};

    glm::vec2 renderTargetSize{};
    glm::vec2 texelSize{};

    glm::vec2 cameraPlanes{1000.0f, 0.1f};

    float deltaTime{};
};
}


#endif //VKTYPES_H
