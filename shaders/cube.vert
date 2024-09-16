#version 450
#extension GL_EXT_buffer_reference : require

struct Model
{
    mat4 modelMatrix;
};

struct Material
{
    vec4 colorFactor;
    vec4 metalRoughFactors;
    ivec4 textureImageIndices;
    ivec4 textureSamplerIndices;
    vec4 alphaCutoff;
};

layout(buffer_reference, std430) readonly buffer ModelData
{
    Model models[];
};

layout(buffer_reference, std430) readonly buffer MaterialData
{
    Material materials[];
};

layout(set = 0, binding = 0) uniform addresses // to be moved to set 2
{
    MaterialData materialBufferDeviceAddress;
    ModelData modelBufferDeviceAddress;
} bufferAddresses;

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec4 color;
layout (location = 3) in vec2 uv;
layout (location = 4) in uint materialIndex;

layout (location = 0) out vec3 outPosition;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec4 outColor;
layout (location = 3) out vec2 outUV;
layout (location = 4) out flat uint outMaterialIndex;


layout (push_constant) uniform PushConstants {
    mat4 vp;
} pushConstants;

void main() {

    mat4 model = bufferAddresses.modelBufferDeviceAddress.models[gl_InstanceIndex].modelMatrix;

    // use world matrix when it becomes available
    outPosition = position;
    outNormal = inverse(transpose(mat3(model))) *  normal;
    outColor = color;
    outUV = uv;
    outMaterialIndex = materialIndex;

    gl_Position = pushConstants.vp * model * vec4(position, 1.0);
}