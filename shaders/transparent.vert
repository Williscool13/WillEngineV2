#version 460
#extension GL_EXT_buffer_reference: require

#include "common.glsl"
#include "scene.glsl"
#include "structure.glsl"

// layout (std140, set = 0, binding = 0) uniform SceneData - scene.glsl

layout (set = 1, binding = 0) uniform Addresses
{
    MaterialData materialBufferDeviceAddress;
    PrimitiveData primitiveBufferDeviceAddress;
    ModelData modelBufferDeviceAddress;
} bufferAddresses;


layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec4 color;
layout (location = 3) in vec2 uv;

layout (location = 0) out vec3 outViewPosition;
layout (location = 1) out vec3 outViewNormal;
layout (location = 2) out vec4 outColor;
layout (location = 3) out vec2 outUV;
layout (location = 4) out flat uint outMaterialIndex;

void main() {
    Primitive primitive = bufferAddresses.primitiveBufferDeviceAddress.primitives[gl_InstanceIndex];
    uint modelIndex = primitive.modelIndex;
    uint materialIndex = primitive.materialIndex;
    Model models = bufferAddresses.modelBufferDeviceAddress.models[modelIndex];

    vec4 viewPos = sceneData.view * models.currentModelMatrix * vec4(position, 1.0);

    outViewPosition = viewPos.xyz;
    outViewNormal = mat3(sceneData.view) * adjugate(models.currentModelMatrix) * normal;
    outColor = color;
    outUV = uv;
    outMaterialIndex = materialIndex;

    vec4 currClipPos = sceneData.proj * viewPos;
    currClipPos.xy += currClipPos.w * sceneData.jitter.xy;
    // do not calculate previous because transparents shouldn't contribute motion vectors

    gl_Position = currClipPos;
}