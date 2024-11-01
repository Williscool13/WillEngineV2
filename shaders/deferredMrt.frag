#version 450
#extension GL_EXT_buffer_reference: require
#extension GL_EXT_nonuniform_qualifier: enable

// world space
layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec4 inColor;
layout (location = 3) in vec2 inUV;
layout (location = 4) in flat uint inMaterialIndex;
layout (location = 5) in vec4 inCurrMvpPosition;
layout (location = 6) in vec4 inPrevMvpPosition;

layout (location = 0) out vec4 normalTarget; // 8,8,8 normal 8 unused
layout (location = 1) out vec4 albedoTarget; // 8,8,8 albedo 8 coverage
layout (location = 2) out vec4 pbrTarget;    // 8 metallic, 8 roughness, 8 emissive (unused), 8 unused
layout (location = 3) out vec2 velocityTarget;    // 16 X, 16 Y


struct Model
{
    mat4 modelMatrix1;
    mat4 modelMatrix2;
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

layout (set = 0, binding = 0) uniform Addresses
{
    MaterialData materialBufferDeviceAddress;
    ModelData modelBufferDeviceAddress;
} bufferAddresses;

layout (set = 1, binding = 0) uniform sampler samplers[];
layout (set = 1, binding = 1) uniform texture2D textures[];

void main() {
    Material m = bufferAddresses.materialBufferDeviceAddress.materials[inMaterialIndex];
    vec3 albedo = vec3(1.0f);

    int colorSamplerIndex = int(m.textureSamplerIndices.x);
    int colorImageIndex = int(m.textureImageIndices.x);
    if (colorSamplerIndex >= 0) {
        albedo = texture(sampler2D(textures[nonuniformEXT(colorImageIndex)], samplers[nonuniformEXT(colorSamplerIndex)]), inUV).xyz;
    }
    albedo = albedo.xyz * inColor.rgb * m.colorFactor.rgb;

    int metalSamplerIndex = int(m.textureSamplerIndices.y);
    int metalImageIndex = int(m.textureImageIndices.y);

    float metallic = m.metalRoughFactors.x;
    float roughness = m.metalRoughFactors.y;
    if (metalSamplerIndex >= 0) {
        vec4 metalRoughSample = texture(sampler2D(textures[nonuniformEXT(metalImageIndex)], samplers[nonuniformEXT(metalSamplerIndex)]), inUV);
        metallic *= metalRoughSample.b;
        roughness *= metalRoughSample.g;
    }

    // check cutoff if applicable

    normalTarget = vec4(normalize(inNormal), 1.0f);
    albedoTarget = vec4(albedo, 1.0f);
    pbrTarget = vec4(metallic, roughness, 0.0f, 0.0f);

    vec2 currNDC = (inCurrMvpPosition.xy / inCurrMvpPosition.w);
    vec2 prevNDC = (inPrevMvpPosition.xy / inPrevMvpPosition.w);
    velocityTarget = (currNDC - prevNDC) * 0.5;
    velocityTarget = vec2(velocityTarget.x, -velocityTarget.y);
}