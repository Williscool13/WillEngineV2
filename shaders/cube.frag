#version 450
#extension GL_EXT_buffer_reference : require

layout (set = 1, binding = 0) uniform sampler samplers[1];
layout (set = 1, binding = 1) uniform texture2D textures[1];

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


void main() {
    Material m = bufferAddresses.materialBufferDeviceAddress.materials[inMaterialIndex];
    int colorSamplerIndex = int(m.textureSamplerIndices.x);
    int colorImageIndex =	 int(m.textureImageIndices.x);
    vec3 _col = texture(sampler2D(textures[colorImageIndex], samplers[colorSamplerIndex]), inUV).xyz;
    /**Material m = bufferAddresses.materialBufferDeviceAddress.materials[inMaterialIndex];
    int colorImageIndex =	int(m.textureImageIndices.x);
    int colorSamplerIndex = int(m.textureSamplerIndices.x);
    vec4 _col = texture(sampler2D(textures[1], samplers[1]), inUV);*/
    //vec4 _col = texture(sampler2D(textures[1], samplers[0]), inUV);

    FragColor = vec4(_col, 1.0f);
    //FragColor = vec4(_col * inColor * m.colorFactor);
}