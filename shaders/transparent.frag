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
layout (location = 0) in vec3 inViewPosition;
layout (location = 1) in vec3 inViewNormal;
layout (location = 2) in vec4 inColor;
layout (location = 3) in vec2 inUV;
layout (location = 4) in flat uint inMaterialIndex;

layout (location = 0) out vec4 accumulationTarget;
layout (location = 1) out float revealageTarget;
layout (location = 2) out vec4 debugTarget;

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
    int disableShadows;
    int disableContactShadows;
} pushConstants;

void main() {
    Material material = bufferAddresses.materials.materialArray[inMaterialIndex];
    vec4 albedo = vec4(1.0f);

    int colorSamplerIndex = material.textureSamplerIndices.x;
    int colorImageIndex = material.textureImageIndices.x;
    if (colorSamplerIndex >= 0) {
        albedo = texture(sampler2D(textures[nonuniformEXT(colorImageIndex)], samplers[nonuniformEXT(colorSamplerIndex)]), inUV);
    }
    albedo = albedo * inColor * material.colorFactor;

    if (albedo.w >= 1.0){
        // Weighted-Blended OIT does not play nice if any fragment has a 1.0 alpha
        albedo.w = 1.0 - TRANSPARENT_ALPHA_EPSILON;
    }

    int metalSamplerIndex = int(material.textureSamplerIndices.y);
    int metalImageIndex = int(material.textureImageIndices.y);

    float metallic = material.metalRoughFactors.x;
    float roughness = material.metalRoughFactors.y;
    if (metalSamplerIndex >= 0) {
        vec4 metalRoughSample = texture(sampler2D(textures[nonuniformEXT(metalImageIndex)], samplers[nonuniformEXT(metalSamplerIndex)]), inUV);
        metallic *= metalRoughSample.b;
        roughness *= metalRoughSample.g;
    }

    vec3 N = normalize(inViewNormal);
    vec3 V = normalize(-inViewPosition);

    // for point lights, light.pos - inPosition
    vec3 L = normalize(mat3(sceneData.view) * normalize(-shadowCascadeData.directionalLightData.direction));
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
    float normalOffsetScale = max(offset, dot(N, L) * offset);
    vec3 offsetPosition = inViewPosition + N * normalOffsetScale;
    vec4 worldPosition = sceneData.invView * vec4(offsetPosition, 1.0f);

    float shadowFactor = getShadowFactorBlend(1, worldPosition.xyz, abs(inViewPosition.z), shadowCascadeData.cascadeSplits, shadowCascadeData.lightViewProj, shadowMapSampler, shadowCascadeData.nearShadowPlane, shadowCascadeData.farShadowPlane);
    if (pushConstants.disableShadows == 1){
        shadowFactor = 1.0f;
    }
    // todo: add contact shadow as descriptor to transparent pipeline
    //float contactShadow = texture(contactShadowBuffer, uv).r;
    float contactShadow = 1.0f;
    if (pushConstants.disableContactShadows == 1){
        contactShadow = 1.0f;
    }

    shadowFactor = min(shadowFactor, contactShadow);

    // Direct lighting with shadows
    vec3 directLight = (diffuse + specular) * nDotL * shadowFactor * shadowCascadeData.directionalLightData.intensity * shadowCascadeData.directionalLightData.color;

    // IBL Reflections
    vec3 worldN = normalize(mat3(sceneData.invView) * N);
    vec3 irradiance = DiffuseIrradiance(environmentDiffuseAndSpecular, worldN);
    vec3 reflectionDiffuse = irradiance * albedo.xyz;
    vec3 reflectionSpecular = SpecularReflection(environmentDiffuseAndSpecular, lut, V, N, sceneData.invView, roughness, F);

    float ao = 1.0f;
    vec3 ambient = (kD * reflectionDiffuse + reflectionSpecular) * ao;
    ambient *= mix(0.4, 1.0, min(shadowFactor, nDotL));
    vec3 finalColor = directLight + ambient;

    float z = gl_FragCoord.z;
    // inverted depth buffer
    float invZ = 1 - z;
    // weight is tweakable, see paper.
    // Weight function from McGuire and Bavoil's paper
    // Parameters can be tweaked for your specific scene needs
    float alphaWeight = clamp(albedo.a * 10.0, 0.01, 1.0);
    float depthWeight = clamp(pow(invZ / 200.0, 4.0), 0.01, 1.0);

    float weight = clamp(pow(alphaWeight, 3.0) * 1.0 / (0.001 + depthWeight), 0.01, 3000.0);

    // RGB = Color * Alpha * Weight, A = Alpha * Weight
    accumulationTarget = vec4(finalColor * albedo.a * weight, albedo.a * weight);
    revealageTarget = albedo.a;

    const float EPSILON = 0.00001f;
    vec3 averageColor = accumulationTarget.rgb / max(accumulationTarget.a, EPSILON);
    vec4 c = vec4(averageColor, 1.0f - revealageTarget);

    debugTarget = c;
}