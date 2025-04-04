#version 460
#extension GL_EXT_buffer_reference: require

#include "scene.glsl"
#include "structure.glsl"

// layout (std140, set = 0, binding = 0) uniform SceneData - scene.glsl

layout (set = 1, binding = 0) uniform Addresses
{
    MaterialData materialBufferDeviceAddress;
    ModelData modelBufferDeviceAddress;
} bufferAddresses;


layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec4 color;
layout (location = 3) in vec2 uv;
layout (location = 4) in uint materialIndex;

layout (location = 0) out vec3 outPosition;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec4 outColor;
layout (location = 3) out vec2 outUV;
layout (location = 4) out flat uint outMaterialIndex;
layout (location = 5) out vec4 outCurrMvpPosition;
layout (location = 6) out vec4 outPrevMvpPosition;

mat3 adjugate(mat4 m) {
    return mat3(
        cross(m[1].xyz, m[2].xyz),
        cross(m[2].xyz, m[0].xyz),
        cross(m[0].xyz, m[1].xyz)
    );

}

void main() {
    Model models = bufferAddresses.modelBufferDeviceAddress.models[gl_InstanceIndex];

    // Current
    vec4 worldPos = models.currentModelMatrix * vec4(position, 1.0);

    outPosition = worldPos.xyz;
    //outNormal = inverse(transpose(mat3(models.currentModelMatrix))) * normal;
    outNormal = adjugate(models.currentModelMatrix) * normal;
    outColor = color;
    outUV = uv;
    outMaterialIndex = materialIndex;

    vec4 currClipPos = sceneData.viewProj * worldPos;
    vec4 prevClipPos = sceneData.prevViewProj * models.previousModelMatrix * vec4(position, 1.0);
    currClipPos.xy += currClipPos.w * sceneData.jitter.xy;
    prevClipPos.xy += prevClipPos.w * sceneData.jitter.zw;
    outCurrMvpPosition = currClipPos;
    outPrevMvpPosition = prevClipPos;

    gl_Position = currClipPos;
}