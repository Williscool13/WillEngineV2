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


layout (location = 0) in vec3 position;


layout (push_constant) uniform PushConstants {
    mat4 viewProj;  // (64)
    // (16)
    // (16)
    // (16)
    // (16)
} pushConstants;

void main() {
    mat4 model = bufferAddresses.modelBufferDeviceAddress.models[gl_InstanceIndex].modelMatrix;
    gl_Position = pushConstants.viewProj * model * vec4(position, 1.0f);
}