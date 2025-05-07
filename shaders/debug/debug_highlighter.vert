#version 460

#include "scene.glsl"

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 outColor;

// layout (std140, set = 0, binding = 0) uniform SceneData - scene.glsl

layout (push_constant) uniform PushConstants {
    mat4 modelMatrix;
    vec4 color;
} push;

void main() {
    gl_Position = sceneData.viewProj * push.modelMatrix * vec4(inPosition, 1.0);
    // fixed depth offset so that the whole object can be seen in most cases
    gl_Position.z += 0.001;
    outColor = push.color.xyz;
}