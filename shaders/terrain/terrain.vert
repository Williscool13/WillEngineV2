#version 460

#include "scene.glsl"

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;

layout (location = 0) out vec3 outPosition;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec4 outColor;
layout (location = 3) out vec2 outUV;
layout (location = 4) out vec4 outCurrMvpPosition;
layout (location = 5) out vec4 outPrevMvpPosition;

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
    //outNormal = adjugate(models.currentModelMatrix) * normal;
    outColor = vec4(normal, 1.0);
    outNormal = normal;
    outUV = uv;

    vec4 currClipPos = sceneData.viewProj * worldPos;
    vec4 prevClipPos = sceneData.prevViewProj * vec4(position, 1.0);
    currClipPos.xy += currClipPos.w * sceneData.jitter.xy;
    prevClipPos.xy += prevClipPos.w * sceneData.jitter.zw;
    outCurrMvpPosition = currClipPos;
    outPrevMvpPosition = prevClipPos;

    gl_Position = currClipPos;
}
