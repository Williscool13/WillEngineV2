#version 450
#extension GL_EXT_buffer_reference : require

layout (set = 1, binding = 0) uniform sampler samplers[32];
layout (set = 1, binding = 1) uniform texture2D textures[255];

// world space
layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec4 inColor;
layout (location = 3) in vec2 inUV;
layout (location = 4) flat in uint inMaterialIndex;

layout (location = 0) out vec4 FragColor;

struct Model
{
    mat4 modelMatrix;
};

struct Material
{
    vec4 color_factor;
    vec4 metal_rough_factors;
    ivec4 texture_image_indices;
    ivec4 texture_sampler_indices;
    vec4 alpha_cutoff; // only use X, can pack with other values in future
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
    uint colorImageIndex =	 m.texture_image_indices.x;
    uint colorSamplerIndex = m.texture_sampler_indices.x;

    vec4 _col = texture(sampler2D(textures[colorImageIndex], samplers[colorSamplerIndex]), inUV);
    FragColor = _col;
}