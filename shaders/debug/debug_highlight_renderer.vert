#version 460

#include "scene.glsl"

layout(location = 0) in vec3 inPosition;

// layout (std140, set = 0, binding = 0) uniform SceneData - scene.glsl

layout (push_constant) uniform PushConstants {
    mat4 modelMatrix;
} push;

void main() {
    gl_Position = sceneData.viewProj * modelMatrix * vec4(inPosition, 1.0);
}