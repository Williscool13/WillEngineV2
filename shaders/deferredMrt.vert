#version 450
#extension GL_EXT_buffer_reference: require

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

layout (set = 0, binding = 0) uniform Addresses
{
    MaterialData materialBufferDeviceAddress;
    ModelData modelBufferDeviceAddress;
} bufferAddresses;

layout (std140, set = 2, binding = 0) uniform SceneData {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    mat4 invView;
    mat4 invProjection;
    mat4 invViewProjection;
    mat4 viewProjCameraLookDirection;
    vec4 cameraPos;
} sceneData;

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

void main() {
    mat4 model = bufferAddresses.modelBufferDeviceAddress.models[gl_InstanceIndex].modelMatrix;
    vec4 mPos = model * vec4(position, 1.0); // why is this red underlined in CLion?!?!?!?!

    // use world matrix when it becomes available
    outPosition = mPos.xyz;
    outNormal = inverse(transpose(mat3(model))) * normal;
    outColor = color;
    outUV = uv;
    outMaterialIndex = materialIndex;

    gl_Position = sceneData.viewProj * mPos;
}