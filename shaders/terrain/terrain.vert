#version 460

#include "scene.glsl"

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;
layout (location = 3) in int materialIndex;

layout (location = 0) out vec3 outPosition;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec2 outUV;
layout (location = 3) flat out int outMaterialIndex;

// layout (std140, set = 0, binding = 0) uniform SceneData - scene.glsl

mat3 adjugate(mat4 m) {
    return mat3(
    cross(m[1].xyz, m[2].xyz),
    cross(m[2].xyz, m[0].xyz),
    cross(m[0].xyz, m[1].xyz)
    );

}

void main() {
    // Model models = bufferAddresses.modelBufferDeviceAddress.models[gl_InstanceIndex];
    // would have a model matrix eventually

    vec4 worldPos = vec4(position, 1.0);

    outPosition = worldPos.xyz;
    outMaterialIndex = materialIndex;
    //outNormal = adjugate(models.currentModelMatrix) * normal;
    outNormal = normal;
    outUV = uv;
    // pass the model position, not the world pos if model matrix is used
    gl_Position = vec4(outPosition, 1.0);
}
