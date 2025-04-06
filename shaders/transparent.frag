#version 450
#extension GL_EXT_buffer_reference: require
#extension GL_EXT_nonuniform_qualifier: enable

#include "scene.glsl"
#include "structure.glsl"


// world space
layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec4 inColor;
layout (location = 3) in vec2 inUV;
layout (location = 4) in flat uint inMaterialIndex;

layout (location = 0) out vec4 accumulationTarget;
layout (location = 1) out float revealageTarget;
layout (location = 2) out vec4 debugTarget;

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
    if (colorSamplerIndex >= 0) {
        albedo = texture(sampler2D(textures[nonuniformEXT(colorImageIndex)], samplers[nonuniformEXT(colorSamplerIndex)]), inUV);
    }
    albedo = albedo * inColor * m.colorFactor;

    if (m.alphaCutoff.y == 1){
        // 1 is "transparent"
        // Transparent fragments will be drawn in the opaque pass if alpha too high
        if (albedo.w >= 0.99) {
            discard;
        }
    }

    float z = gl_FragCoord.z;
    // inverted depth buffer
    float invZ = 1 - z;
    // weight is tweakable, see paper.
    float weight = clamp(pow(min(1.0, albedo.a * 10.0) + 0.01, 3.0) * 1.0 / (0.001 + pow(invZ / 200.0, 4.0)), 0.01, 3000.0);

    accumulationTarget = vec4(albedo.rgb * albedo.a * weight, albedo.a * weight);
    revealageTarget = albedo.a * weight;

    debugTarget = vec4(1.0f);

    //    int metalSamplerIndex = int(m.textureSamplerIndices.y);
    //    int metalImageIndex = int(m.textureImageIndices.y);
    //
    //    float metallic = m.metalRoughFactors.x;
    //    float roughness = m.metalRoughFactors.y;
    //    if (metalSamplerIndex >= 0) {
    //        vec4 metalRoughSample = texture(sampler2D(textures[nonuniformEXT(metalImageIndex)], samplers[nonuniformEXT(metalSamplerIndex)]), inUV);
    //        metallic *= metalRoughSample.b;
    //        roughness *= metalRoughSample.g;
    //    }


    //    normalTarget = vec4(normalize(inNormal), 0.0f);
    //    albedoTarget = vec4(albedo.xyz, 1.0f);
}