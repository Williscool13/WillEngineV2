#version 450
#extension GL_EXT_buffer_reference: require
#extension GL_EXT_nonuniform_qualifier: enable

#include "common.glsl"
#include "scene.glsl"
#include "structure.glsl"
#include "transparent.glsl"


// world space
layout (location = 0) in vec3 inViewPosition;
layout (location = 1) in vec3 inViewNormal;
layout (location = 2) in vec4 inColor;
layout (location = 3) in vec2 inUV;
layout (location = 4) in flat uint inMaterialIndex;
layout (location = 5) in flat uint inHasTransparent;
layout (location = 6) in vec4 inCurrMvpPosition;
layout (location = 7) in vec4 inPrevMvpPosition;

layout (location = 0) out vec4 normalTarget;// 10,10,10, (2 unused)
layout (location = 1) out vec4 albedoTarget;// 10,10,10, (2 -> 0 = environment Map, 1 = renderObject)
layout (location = 2) out vec4 pbrTarget; // metal/roughness/unused/unused
layout (location = 3) out vec2 velocityTarget;

// layout (std140, set = 0, binding = 0) uniform SceneData - scene.glsl

layout (set = 1, binding = 0) uniform Addresses
{
    Instances instances;
    Models models;
    PrimitiveData primitives;
    Materials materials;

} bufferAddresses;

layout (set = 2, binding = 0) uniform sampler samplers[];
layout (set = 2, binding = 1) uniform texture2D textures[];

void main() {
    Material material = bufferAddresses.materials.materialArray[inMaterialIndex];
    vec4 albedo = vec4(1.0f);

    int colorSamplerIndex = material.textureSamplerIndices.x;
    int colorImageIndex = material.textureImageIndices.x;
    if (colorSamplerIndex > -1 && colorImageIndex > -1) {
        vec2 colorUv = inUV * material.colorUvTransform.xy + material.colorUvTransform.zw;
        albedo = texture(sampler2D(textures[nonuniformEXT(colorImageIndex)], samplers[nonuniformEXT(colorSamplerIndex)]), colorUv);
    }
    albedo = albedo * inColor * material.colorFactor;

    // 2 is "mask" (cutout) blend type
    if (material.alphaProperties.y == 2){
        // x is the cutout value (if any)
        if (albedo.w < material.alphaProperties.x){
            discard;
        }
    }

    int metalSamplerIndex = int(material.textureSamplerIndices.y);
    int metalImageIndex = int(material.textureImageIndices.y);

    float metallic = material.metalRoughFactors.x;
    float roughness = material.metalRoughFactors.y;
    if (metalSamplerIndex > -1 && metalImageIndex > -1) {
        vec2 metalRoughUv = inUV * material.metalRoughUvTransform.xy + material.metalRoughUvTransform.zw;
        vec4 metalRoughSample = texture(sampler2D(textures[nonuniformEXT(metalImageIndex)], samplers[nonuniformEXT(metalSamplerIndex)]), metalRoughUv);
        metallic *= metalRoughSample.b;
        roughness *= metalRoughSample.g;
    }

    normalTarget = vec4(packNormal(normalize(inViewNormal)), 0.0f);
    albedoTarget = vec4(albedo.xyz, 1.0f);
    pbrTarget = vec4(metallic, roughness, 0.0f, inHasTransparent);

    vec2 currNdc = inCurrMvpPosition.xy / inCurrMvpPosition.w;
    vec2 prevNdc = inPrevMvpPosition.xy / inPrevMvpPosition.w;

    vec2 velocity = (currNdc - prevNdc) * 0.5f;
    velocityTarget = velocity;
}