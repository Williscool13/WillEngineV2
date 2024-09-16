#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : enable

layout (set = 1, binding = 0) uniform sampler samplers[];
layout (set = 1, binding = 1) uniform texture2D textures[];

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

layout (set = 0, binding = 0) uniform addresses // to be moved to set 2
{
    MaterialData materialBufferDeviceAddress;
    ModelData modelBufferDeviceAddress;
} bufferAddresses;

vec3 linearToSRGB(vec3 linearColor) {
    return pow(linearColor, vec3(1.0 / 2.2));
}

void main() {
    Material m = bufferAddresses.materialBufferDeviceAddress.materials[inMaterialIndex];
    vec3 albedo = vec3(1.0f);

    int colorSamplerIndex = int(m.textureSamplerIndices.x);
    int colorImageIndex =	 int(m.textureImageIndices.x);
    if (colorSamplerIndex >= 0){
        albedo = texture(sampler2D(textures[nonuniformEXT(colorImageIndex)], samplers[nonuniformEXT(colorSamplerIndex)]), inUV).xyz;
    }
    albedo *= inColor.rgb * m.colorFactor.rgb;
    vec3 lightDir = normalize(vec3(1.0, -0.75, 1.0));
    vec3 normal = normalize(inNormal);
    float diffuseFactor = max(dot(normal, lightDir), 0.0);

    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * albedo;
    vec3 finalColor = (ambient + diffuseFactor * albedo);
    vec3 gammaCorrectColor = linearToSRGB(finalColor);
    FragColor = vec4(gammaCorrectColor, 1.0f);
}