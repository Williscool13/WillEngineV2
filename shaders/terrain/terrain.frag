#version 460

#extension GL_EXT_buffer_reference: require
#extension GL_EXT_nonuniform_qualifier: enable

#include "scene.glsl"
#include "structure.glsl"


// world space
layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;
layout (location = 3) flat in int inMaterialIndex;
layout (location = 4) in vec4 inColor;
layout (location = 5) in vec4 inCurrMvpPosition;
layout (location = 6) in vec4 inPrevMvpPosition;

layout (location = 0) out vec4 normalTarget;// 8,8,8 normal 8 unused
layout (location = 1) out vec4 albedoTarget;// 8,8,8 albedo 8 coverage
layout (location = 2) out vec4 pbrTarget;// 8 metallic, 8 roughness, 8 emissive (unused), 8 unused
layout (location = 3) out vec2 velocityTarget;// 16 X, 16 Y

// layout (std140, set = 0, binding = 0) uniform SceneData - scene.glsl

layout (set = 1, binding = 0) uniform sampler2D textures[];

layout (std140, set = 2, binding = 0) uniform TerrainProperties {
    float slopeRockThreshold;
    float slopeRockBlend;
    float heightSandThreshold;
    float heightSandBlend;
    float heightGrassThreshold;
    float heightGrassBlend;
    float minHeight;
    float maxHeight;
} terrainProperties;

float smoothBlend(float value, float threshold, float range) {
    return 1.0 - smoothstep(threshold - range, threshold + range, value);
}

void main() {
    normalTarget = vec4(normalize(inNormal), 0.0f);

    float slope = 1.0 - abs(dot(normalize(inNormal), vec3(0.0, 1.0, 0.0)));
    float height = inPosition.y;

    float normalizedHeight = (height - terrainProperties.minHeight) / (terrainProperties.maxHeight - terrainProperties.minHeight);

    // Texture Weights
    float sandWeight = smoothBlend(normalizedHeight, terrainProperties.heightSandThreshold, terrainProperties.heightSandBlend);
    float rockWeight = smoothBlend(1.0 - slope, terrainProperties.slopeRockThreshold, terrainProperties.slopeRockBlend);
    float grassWeight = 1.0 - sandWeight - rockWeight;

    // Optional: Reduce grass at high elevations (for snow or other high elevation textures)
    grassWeight *= smoothBlend(normalizedHeight, terrainProperties.heightGrassThreshold, terrainProperties.heightGrassBlend);

    // Normalize weights to ensure they sum to 1.0
    float totalWeight = sandWeight + rockWeight + grassWeight;
    sandWeight /= totalWeight;
    rockWeight /= totalWeight;
    grassWeight /= totalWeight;

    // Sample textures
    vec4 grassColor = texture(textures[0], inUV);
    vec4 rockColor = texture(textures[1], inUV);
    vec4 sandColor = texture(textures[2], inUV);

    // Blend textures based on weights
    vec4 color = grassColor * grassWeight + rockColor * rockWeight + sandColor * sandWeight;

    // Apply vertex color modulation
    color *= inColor;

    albedoTarget = vec4(color.xyz, 1.0);

    // Adjust PBR properties based on the dominant material
    float roughness = mix(0.8, 0.9, rockWeight) + mix(0.0, -0.3, sandWeight); // Rocks rougher, sand smoother
    pbrTarget = vec4(0.0, roughness, 0.0, 0.0); // 0 metallic, varying roughness

    // Calculate velocity for temporal AA
    vec2 currNdc = inCurrMvpPosition.xy / inCurrMvpPosition.w;
    vec2 prevNdc = inPrevMvpPosition.xy / inPrevMvpPosition.w;

    vec2 velocity = (currNdc - prevNdc) * 0.5f;
    velocityTarget = velocity;

//    normalTarget = vec4(normalize(inNormal), 0.0f);
//    vec4 color = texture(textures[inMaterialIndex], inUV);
//    color *= inColor;
//
//    albedoTarget = color;
//    pbrTarget = vec4(0.0f, 0.8f, 0.0f, 0.0f);// 0 metallic, 0.5 roughness
//
//    vec2 currNdc = inCurrMvpPosition.xy / inCurrMvpPosition.w;
//    vec2 prevNdc = inPrevMvpPosition.xy / inPrevMvpPosition.w;
//
//    vec2 jitterDiff = (sceneData.jitter.xy - sceneData.jitter.zw);
//    //vec2 velocity = (currNdc - prevNdc - jitterDiff);
//    vec2 velocity = (currNdc - prevNdc) * 0.5f;
//    velocityTarget = velocity;
}
