#version 450

#include "common.glsl"
#include "scene.glsl"

layout (location = 0) in vec3 uv;
layout (location = 1) in vec4 inCurrMvpPosition;
layout (location = 2) in vec4 inPrevMvpPosition;

layout (location = 0) out vec4 normalTarget;// 10,10,10, (2 unused)
layout (location = 1) out vec4 albedoTarget;// 10,10,10, (2 -> 0 = environment Map, 1 = renderObject)
layout (location = 2) out vec4 pbrTarget;// 8 metallic, 8 roughness, 8 emissive (unused), 8 unused
layout (location = 3) out vec2 velocityTarget;// 16 X, 16 Y

// layout (std140, set = 0, binding = 0) uniform SceneData - scene.glsl

layout(set = 1, binding = 0) uniform samplerCube environmentMap;

layout (push_constant) uniform PushConstants {
    float sunSize;
    float sunFalloff;
} push;


void main()
{
    vec3 direction = normalize(uv);
    direction.y = -direction.y;
    vec3 envColor = textureLod(environmentMap, direction, 0).rgb;

    vec3 sunDir = -sceneData.directionalLightData.direction;
    float sunDot = dot(normalize(uv), sunDir);

    if (sunDot > push.sunSize) {
        float sunIntensity = smoothstep(push.sunSize, push.sunFalloff, sunDot);
        vec3 sunColor = sceneData.directionalLightData.color * sceneData.directionalLightData.intensity;
        envColor = mix(envColor, sunColor * 10.0, sunIntensity);
    }


    // 0 = "do not calculate lighting" flag
    albedoTarget = vec4(envColor, 0.0);

    normalTarget = vec4(packNormal(mat3(sceneData.view) * -direction), 0.0f);
    pbrTarget = vec4(0.0f);

    vec2 currNdc = inCurrMvpPosition.xy / inCurrMvpPosition.w;
    vec2 prevNdc = inPrevMvpPosition.xy / inPrevMvpPosition.w;
    vec2 velocity = (currNdc - prevNdc) * 0.5f;
    velocityTarget = velocity;
}