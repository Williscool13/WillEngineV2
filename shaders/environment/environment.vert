#version 450

#include "scene.glsl"

layout (location = 0) out vec3 uv;
layout (location = 1) out vec4 outCurrMvpPosition;
layout (location = 2) out vec4 outPrevMvpPosition;

// layout (std140, set = 0, binding = 0) uniform SceneData - scene.glsl

void main() {
    const vec3 vertices[3] = vec3[3](vec3(-1, -1, 0.000001), vec3(3, -1, 0.000001), vec3(-1, 3, 0.000001));


    vec4 currClipPos = vec4(vertices[gl_VertexIndex], 1);
    vec3 currWorldPos = (inverse(sceneData.viewProjCameraLookDirection) * currClipPos).xyz;
    gl_Position = currClipPos;
    gl_Position.xy += gl_Position.w * sceneData.jitter.xy;
    uv = currWorldPos;

    vec4 prevClipPos = sceneData.prevViewProjCameraLookDirection * vec4(currWorldPos, 1.0f);

    currClipPos.xy += currClipPos.w * sceneData.jitter.xy;
    prevClipPos.xy += prevClipPos.w * sceneData.jitter.zw;
    outCurrMvpPosition = currClipPos;
    outPrevMvpPosition = prevClipPos;
}