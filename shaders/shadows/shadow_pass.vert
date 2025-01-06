#version 460
#extension GL_EXT_buffer_reference: require

//#include "scene.glsl"
#include "structure.glsl"

// layout (std140, set = 0, binding = 0) uniform SceneData - scene.glsl

layout (set = 0, binding = 0) uniform Addresses
{
    MaterialData materialBufferDeviceAddress;
    ModelData modelBufferDeviceAddress;
} bufferAddresses;

layout (location = 0) in vec3 position;

layout (push_constant) uniform PushConstants {
    mat4 lightSpaceMatrix;
} push;

void main() {
    Model models = bufferAddresses.modelBufferDeviceAddress.models[gl_InstanceIndex];
    gl_Position = push.lightSpaceMatrix * models.currentModelMatrix * vec4(position, 1.0);
}