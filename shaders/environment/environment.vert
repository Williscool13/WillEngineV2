#version 450

#include "scene.glsl"

layout (location = 0) out vec3 uv;
layout (location = 1) out vec4 outCurrMvpPosition;
layout (location = 2) out vec4 outPrevMvpPosition;

// layout (std140, set = 0, binding = 0) uniform SceneData - scene.glsl

void main() {
    const vec3 vertices[3] = vec3[3](vec3(-1, -1, 0.00001), vec3(3, -1, 0.00001), vec3(-1, 3, 0.00001));


    vec4 currClipPos = vec4(vertices[gl_VertexIndex], 1);
    vec4 prevClipPos = vec4(vertices[gl_VertexIndex], 1);
    currClipPos.xy += currClipPos.w * sceneData.jitter.xy;
    prevClipPos.xy += prevClipPos.w * sceneData.jitter.zw;
    outCurrMvpPosition = currClipPos;
    outPrevMvpPosition = prevClipPos;

    gl_Position = currClipPos;
    uv = (inverse(sceneData.viewProjCameraLookDirection) * gl_Position).xyz;
}