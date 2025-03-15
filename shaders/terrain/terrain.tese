#version 450

#include "scene.glsl"

layout(quads, equal_spacing, ccw) in;

layout(location = 0) in vec3 inPosition[];
layout(location = 1) in vec3 inNormal[];
layout(location = 2) in vec2 inTexCoord[];
layout(location = 3) flat in int inMaterialIndex[];

layout(location = 0) out vec3 outPosition;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outTexCoord;
layout(location = 3) flat out int outMaterialIndex;
layout(location = 4) out vec4 outCurrMvpPosition;
layout(location = 5) out vec4 outPrevMvpPosition;

// layout (std140, set = 0, binding = 0) uniform SceneData - scene.glsl

void main() {
    float u = gl_TessCoord.x;
    float v = gl_TessCoord.y;

    vec3 pos0 = mix(inPosition[0], inPosition[1], u);
    vec3 pos1 = mix(inPosition[2], inPosition[3], u);
    outPosition = mix(pos0, pos1, v);

    vec3 normal0 = mix(inNormal[0], inNormal[1], u);
    vec3 normal1 = mix(inNormal[2], inNormal[3], u);
    outNormal = normalize(mix(normal0, normal1, v));

    vec2 tex0 = mix(inTexCoord[0], inTexCoord[1], u);
    vec2 tex1 = mix(inTexCoord[2], inTexCoord[3], u);
    outTexCoord = mix(tex0, tex1, v);

    outMaterialIndex = inMaterialIndex[0];

    vec4 currClipPos = sceneData.viewProj * vec4(outPosition, 1.0);
    vec4 prevClipPos = sceneData.prevViewProj * vec4(outPosition, 1.0);
    //vec4 currClipPos = sceneData.viewProj * modelMatrix * vec4(outPosition, 1.0);
    //vec4 prevClipPos = sceneData.prevViewProj * prevModelMatrix * vec4(outPosition, 1.0);
    currClipPos.xy += currClipPos.w * sceneData.jitter.xy;
    prevClipPos.xy += prevClipPos.w * sceneData.jitter.zw;
    outCurrMvpPosition = currClipPos;
    outPrevMvpPosition = prevClipPos;

    gl_Position = outCurrMvpPosition;
}