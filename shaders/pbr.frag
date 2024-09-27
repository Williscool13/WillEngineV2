#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : enable

#include "pbr.glsl"

// world space
layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec4 inColor;
layout (location = 3) in vec2 inUV;
layout (location = 4) in flat uint inMaterialIndex;

layout (location = 0) out vec4 FragColor;

struct Model
{
    mat4 modelMatrix;
};

struct Material
{
    vec4 colorFactor;
    vec4 metalRoughFactors;
    vec4 textureImageIndices;
    vec4 textureSamplerIndices;
    vec4 alphaCutoff;
};

layout (buffer_reference, std430) readonly buffer ModelData
{
    Model models[];
};

layout (buffer_reference, std430) readonly buffer MaterialData
{
    Material materials[];
};

layout (set = 0, binding = 0) uniform addresses
{
    MaterialData materialBufferDeviceAddress;
    ModelData modelBufferDeviceAddress;
} bufferAddresses;

layout (set = 1, binding = 0) uniform sampler samplers[];
layout (set = 1, binding = 1) uniform texture2D textures[];

/**layout(set = 2, binding = 0) uniform sceneUniforms
{
    //mat4 view;
    //mat4 proj;
    //mat4 viewproj;
    //vec4 cameraPos; // w is for alignment
} sceneData;*/

layout(set = 3, binding = 0) uniform samplerCube environmentDiffuseAndSpecular;
layout(set = 3, binding = 1) uniform sampler2D lut;

layout (push_constant) uniform PushConstants {
    mat4 viewProj;  // (64)
    vec4 cameraPos; // (16) - w is for alignment
    // (16)
    // (16)
    // (16)
} pushConstants;

vec3 linearToSRGB(vec3 linearColor) {
    return pow(linearColor, vec3(1.0 / 2.2));
}


const float MAX_REFLECTION_LOD = 4.0;
const uint DIFF_IRRA_MIP_LEVEL = 5;
const bool FLIP_ENVIRONMENT_MAP_Y = true;

vec3 DiffuseIrradiance(vec3 N)
{
    vec3 ENV_N = N;
    if (FLIP_ENVIRONMENT_MAP_Y) { ENV_N.y = -ENV_N.y; }
    return textureLod(environmentDiffuseAndSpecular, ENV_N, DIFF_IRRA_MIP_LEVEL).rgb;
}

vec3 SpecularReflection(vec3 V, vec3 N, float roughness, vec3 F) {
    vec3 R = reflect(-V, N);
    if (FLIP_ENVIRONMENT_MAP_Y) { R.y = -R.y; }
    // dont need to skip mip 5 because never goes above 4
    vec3 prefilteredColor = textureLod(environmentDiffuseAndSpecular, R, roughness * MAX_REFLECTION_LOD).rgb;

    vec2 envBRDF = texture(lut, vec2(max(dot(N, V), 0.0f), roughness)).rg;

    return prefilteredColor * (F * envBRDF.x + envBRDF.y);
}


void main() {
    Material m = bufferAddresses.materialBufferDeviceAddress.materials[inMaterialIndex];
    vec4 albedo = vec4(1.0f);

    int colorSamplerIndex = int(m.textureSamplerIndices.x);
    int colorImageIndex =	 int(m.textureImageIndices.x);
    if (colorSamplerIndex >= 0){
        albedo = texture(sampler2D(textures[nonuniformEXT(colorImageIndex)], samplers[nonuniformEXT(colorSamplerIndex)]), inUV);
    }
    vec3 _albedoTemp = albedo.xyz * inColor.rgb * m.colorFactor.rgb;
    albedo = vec4(_albedoTemp, albedo.w);

    int metalSamplerIndex = int(m.textureSamplerIndices.y);
    int metalImageIndex = int(m.textureImageIndices.y);

    float metallic = m.metalRoughFactors.x;
    float roughness = m.metalRoughFactors.y;
    if (metalSamplerIndex >= 0){
        vec4 metalRoughSample = texture(sampler2D(textures[nonuniformEXT(metalImageIndex)], samplers[nonuniformEXT(metalSamplerIndex)]), inUV);
        metallic *= metalRoughSample.b;
        roughness *= metalRoughSample.g;
    }

    // check cutoff if applicable

    const vec3 hardCodedMainLightColor = vec3(1.0f);
    const vec3 hardCodedMainLightDirection = normalize(vec3(1.0, -0.75, 1.0));
    vec3 N = normalize(inNormal);
    vec3 V = normalize(pushConstants.cameraPos.xyz - inPosition);
    vec3 L = normalize(hardCodedMainLightDirection); // for point lights, light.pos - inPosition
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

    // REFLECTIONS
    vec3 irradiance = DiffuseIrradiance(N);
    vec3 reflection_diffuse = irradiance * albedo.xyz;

    vec3 reflection_specular = SpecularReflection(V, N, roughness, F);
    vec3 ambient = (kD * reflection_diffuse + reflection_specular);

    vec3 finalColor = (diffuse + specular) * hardCodedMainLightColor * nDotL;
    finalColor += ambient;

    /**vec3 corrected_ambient = ambient / (ambient + vec3(1.0f)); // Reinhard
    corrected_ambient = pow(corrected_ambient, vec3(1.0f / 2.2f)); // gamma correction
    finalColor += corrected_ambient;

    FragColor = vec4(finalColor, albedo.w);*/

    vec3 correctedFinalColor = finalColor / (finalColor + vec3(1.0f)); // Reinhard
    correctedFinalColor = pow(correctedFinalColor, vec3(1.0f / 2.2f)); // gamma correction

    FragColor = vec4(correctedFinalColor, albedo.w);

}