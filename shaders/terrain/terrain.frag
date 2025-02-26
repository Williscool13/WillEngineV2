#version 460

#extension GL_EXT_buffer_reference: require
#extension GL_EXT_nonuniform_qualifier: enable

#include "scene.glsl"
#include "structure.glsl"


// world space
layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec4 inColor;
layout (location = 3) in vec2 inUV;
layout (location = 4) in vec4 inCurrMvpPosition;
layout (location = 5) in vec4 inPrevMvpPosition;

layout (location = 0) out vec4 normalTarget; // 8,8,8 normal 8 unused
layout (location = 1) out vec4 albedoTarget; // 8,8,8 albedo 8 coverage
layout (location = 2) out vec4 pbrTarget;    // 8 metallic, 8 roughness, 8 emissive (unused), 8 unused
layout (location = 3) out vec2 velocityTarget;    // 16 X, 16 Y

// layout (std140, set = 0, binding = 0) uniform SceneData - scene.glsl

void main() {

    normalTarget = vec4(normalize(inNormal), 0.0f);
    albedoTarget = vec4(inColor.xyz, 1.0f);
    pbrTarget = vec4(0.0f, 0.5f, 0.0f, 0.0f); // 0 metallic, 0.5 roughness

    vec2 currNdc = inCurrMvpPosition.xy / inCurrMvpPosition.w;
    vec2 prevNdc = inPrevMvpPosition.xy / inPrevMvpPosition.w;

    vec2 jitterDiff = (sceneData.jitter.xy - sceneData.jitter.zw);
    vec2 velocity = (currNdc - prevNdc - jitterDiff) * 0.5;
    velocityTarget = velocity;
}
