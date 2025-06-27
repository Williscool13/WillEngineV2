#version 460
#extension GL_EXT_buffer_reference: require

#include "common.glsl"
#include "scene.glsl"
#include "structure.glsl"

// layout (std140, set = 0, binding = 0) uniform SceneData - scene.glsl

layout (set = 1, binding = 0) uniform Addresses
{
    Instances instances;
    Models models;
    PrimitiveData primitives;
    Materials materials;
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
layout (location = 5) out flat uint outHasTransparent;
layout (location = 6) out vec4 outCurrMvpPosition;
layout (location = 7) out vec4 outPrevMvpPosition;

void main() {
    Instance instances = bufferAddresses.instances.instanceArray[gl_InstanceIndex];

    Primitive primitive = bufferAddresses.primitives.primitiveArray[instances.primitiveDataIndex];
    Model model = bufferAddresses.models.modelArray[instances.modelIndex];

    Material material = bufferAddresses.materials.materialArray[primitive.materialIndex];


    vec4 viewPos = sceneData.view * model.currentModelMatrix * vec4(position, 1.0);

    outViewPosition = viewPos.xyz;
    outViewNormal = mat3(sceneData.view) * adjugate(model.currentModelMatrix) * normal;
    outColor = color;
    outUV = uv;
    outMaterialIndex = primitive.materialIndex;
    outHasTransparent = primitive.bHasTransparent;

    vec4 currClipPos = sceneData.proj * viewPos;
    vec4 prevClipPos = sceneData.prevViewProj * model.previousModelMatrix * vec4(position, 1.0);
    currClipPos.xy += currClipPos.w * sceneData.jitter.xy;
    prevClipPos.xy += prevClipPos.w * sceneData.jitter.zw;
    outCurrMvpPosition = currClipPos;
    outPrevMvpPosition = prevClipPos;

    gl_Position = currClipPos;
}