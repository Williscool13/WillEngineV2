#version 460

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 outColor;

#include "scene.glsl"

struct BoxInstance{
    mat4 transform;
    vec3 color;
    float padding;
};

// layout (std140, set = 0, binding = 0) uniform SceneData - scene.glsl

layout(std140, set = 1, binding = 0) readonly buffer BoxInstanceBuffer {
    BoxInstance instances[];
} boxInstances;

void main() {
    BoxInstance instance = boxInstances.instances[gl_InstanceIndex];

    gl_Position = sceneData.viewProj * vec4(inPosition, 1.0);
    gl_Position.xy += gl_Position.w * sceneData.jitter.xy;
    outColor = instance.color;
}