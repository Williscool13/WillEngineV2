#version 450
#extension GL_EXT_buffer_reference: require
#extension GL_EXT_nonuniform_qualifier: enable

#include "common.glsl"
#include "scene.glsl"
#include "structure.glsl"
#include "pbr.glsl"
#include "environment.glsl"
#include "lights.glsl"
#include "shadows.glsl"
#include "transparent.glsl"

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

layout (set = 3, binding = 0) uniform samplerCube environmentDiffuseAndSpecular;
layout (set = 3, binding = 1) uniform sampler2D lut;

layout (std140, set = 4, binding = 0) uniform ShadowCascadeData {
    CascadeSplit cascadeSplits[4];
    mat4 lightViewProj[4];
    DirectionalLight directionalLightData; // w is intensity
    float nearShadowPlane;
    float farShadowPlane;
    vec2 pad;
} shadowCascadeData;

layout (set = 5, binding = 0) uniform sampler2DShadow shadowMapSampler[4];

layout (push_constant) uniform PushConstants {
    int enabled;
    int receiveShadows;
} pushConstants;

void main() {
    Material m = bufferAddresses.materialBufferDeviceAddress.materials[inMaterialIndex];
    vec4 albedo = vec4(1.0f);

    int colorSamplerIndex = m.textureSamplerIndices.x;
    int colorImageIndex = m.textureImageIndices.x;
    if (colorSamplerIndex >= 0) {
        albedo = texture(sampler2D(textures[nonuniformEXT(colorImageIndex)], samplers[nonuniformEXT(colorSamplerIndex)]), inUV);
    }
    albedo = albedo * inColor * m.colorFactor;

    if (m.alphaCutoff.y == 1) {
        // Draw only if alpha is sufficiently less than 1
        if (albedo.w > 1.0 - TRANSPARENT_ALPHA_EPSILON) {
            discard;
        }
    }

    int metalSamplerIndex = int(m.textureSamplerIndices.y);
    int metalImageIndex = int(m.textureImageIndices.y);

    float metallic = m.metalRoughFactors.x;
    float roughness = m.metalRoughFactors.y;
    if (metalSamplerIndex >= 0) {
        vec4 metalRoughSample = texture(sampler2D(textures[nonuniformEXT(metalImageIndex)], samplers[nonuniformEXT(metalSamplerIndex)]), inUV);
        metallic *= metalRoughSample.b;
        roughness *= metalRoughSample.g;
    }

    vec3 N = normalize(inNormal);
    vec3 V = normalize(sceneData.cameraPos.xyz - inPosition);

    vec3 L = normalize(-shadowCascadeData.directionalLightData.direction); // for point lights, light.pos - inPosition
    vec3 H = normalize(V + L);

    // SPECULAR
    float NDF = D_GGX(N, H, roughness);
    float G = G_SCHLICKGGX_SMITH(N, V, L, roughness);
    vec3 F0 = mix(vec3(0.04), albedo.xyz, metallic);
    vec3 F = F_SCHLICK(V, H, F0);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0f * max(dot(N, V), 0.0f) * max(dot(N, L), 0.0f);
    vec3 specular = numerator / max(denominator, 0.001f);

    vec3 kS = F;
    vec3 kD = vec3(1.0f) - kS;
    kD *= 1.0f - metallic;

    // DIFFUSE
    float nDotL = max(dot(N, L), 0.0f);
    vec3 diffuse = Lambert(kD, albedo.xyz);

    // SHADOWS
    float offset = 0.05f;
    float normalOffsetScale = max(offset, dot(inNormal, L) * offset);
    vec3 offsetPosition = inPosition + inNormal * normalOffsetScale;
    float _tempShadowFactor = getShadowFactorBlend(1, inPosition, sceneData.view, shadowCascadeData.cascadeSplits, shadowCascadeData.lightViewProj, shadowMapSampler, shadowCascadeData.nearShadowPlane, shadowCascadeData.farShadowPlane);
    float shadowFactor = clamp(smoothstep(-0.15f, 0.5f, dot(N, L)) * _tempShadowFactor, 0, 1);

    if (pushConstants.receiveShadows == 0) {
        shadowFactor = 1.0f;
    }

    float indirectAttenuation = mix(0.35, 1.0, shadowFactor);

    // IBL REFLECTIONS
    vec3 irradiance = DiffuseIrradiance(environmentDiffuseAndSpecular, N);
    vec3 reflectionDiffuse = irradiance * albedo.xyz * indirectAttenuation;

    vec3 reflectionSpecular = SpecularReflection(environmentDiffuseAndSpecular, lut, V, N, roughness, F) * indirectAttenuation;

    float ao = 1.0f;
    vec3  ambient = (kD * reflectionDiffuse + reflectionSpecular) * ao;

    vec3 finalColor = (diffuse + specular) * nDotL * shadowFactor * shadowCascadeData.directionalLightData.intensity * shadowCascadeData.directionalLightData.color;
    finalColor += ambient;

    float z = gl_FragCoord.z;
    // inverted depth buffer
    float invZ = 1 - z;
    // weight is tweakable, see paper.
    // Weight function from McGuire and Bavoil's paper
    // Parameters can be tweaked for your specific scene needs
    float alphaWeight = clamp(albedo.a * 10.0, 0.01, 1.0);
    float depthWeight = clamp(pow(invZ / 200.0, 4.0), 0.01, 1.0);

    // Final weight calculation - you can adjust this formula for your needs
    float weight = clamp(pow(alphaWeight, 3.0) * 1.0 / (0.001 + depthWeight), 0.01, 3000.0);

    // RGB = Color * Alpha * Weight, A = Alpha * Weight
    accumulationTarget = vec4(finalColor * albedo.a * weight, albedo.a * weight);
    revealageTarget = albedo.a;

    const float EPSILON = 0.00001f;
    vec3 averageColor = accumulationTarget.rgb / max(accumulationTarget.a, EPSILON);
    vec4 c = vec4(averageColor, 1.0f - revealageTarget);

    debugTarget = c;
}