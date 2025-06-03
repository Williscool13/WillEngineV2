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
    MaterialData materialBufferDeviceAddress;
    ModelData modelBufferDeviceAddress;
} bufferAddresses;

layout (set = 2, binding = 0) uniform sampler samplers[];
layout (set = 2, binding = 1) uniform texture2D textures[];

void main() {
    Material m = bufferAddresses.materialBufferDeviceAddress.materials[inMaterialIndex];
    vec4 albedo = vec4(1.0f);

    int colorSamplerIndex = m.textureSamplerIndices.x;
    int colorImageIndex = m.textureImageIndices.x;
    if (colorSamplerIndex > -1 && colorImageIndex > -1) {
        vec2 colorUv = inUV * m.colorUvTransform.xy + m.colorUvTransform.zw;
        albedo = texture(sampler2D(textures[nonuniformEXT(colorImageIndex)], samplers[nonuniformEXT(colorSamplerIndex)]), colorUv);
        albedo.xyz = linear2srgb(albedo.xyz);
    }
    //albedo = albedo * inColor * m.colorFactor;

    // Look into custom shaders specifically for these? More draw commands vs branching...
    // 1 is transparent blend type
    if (m.alphaProperties.y == 1) {
        // Draw only if alpha is close enough to 1
        if (albedo.w <= 1.0 - TRANSPARENT_ALPHA_EPSILON) {
            discard;
        }
    }

    // 2 is "mask" (cutout) blend type
    if (m.alphaProperties.y == 2){
        // 2 is "mask" blend type
        if (albedo.w < m.alphaProperties.x){
            discard;
        }
    }

    int metalSamplerIndex = int(m.textureSamplerIndices.y);
    int metalImageIndex = int(m.textureImageIndices.y);

    float metallic = m.metalRoughFactors.x;
    float roughness = m.metalRoughFactors.y;
    if (metalSamplerIndex > -1 && metalImageIndex > -1) {
        vec2 metalRoughUv = inUV * m.metalRoughUvTransform.xy + m.metalRoughUvTransform.zw;
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