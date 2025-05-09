#version 460

#include "scene.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 outColor;

// layout (std140, set = 0, binding = 0) uniform SceneData - scene.glsl

void main() {
    gl_Position = sceneData.viewProj * vec4(inPosition, 1.0);
    gl_Position.xy += gl_Position.w * sceneData.jitter.xy;
    outColor = inColor;
}