#version 450
#extension GL_EXT_buffer_reference: require

struct Model
{
    mat4 modelMatrix1;
    mat4 modelMatrix2;
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
    vec4 cameraPos;
    mat4 viewProjCameraLookDirection;

    mat4 invView;
    mat4 invProjection;
    mat4 invViewProjection;

    mat4 prevView;
    mat4 prevProj;
    mat4 prevViewProj;

    vec4 jitter;

    vec2 renderTargetSize;
    int frameNumber; // either 0 or 1
    float deltaTime;
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
layout (location = 5) out vec4 outCurrMvpPosition;
layout (location = 6) out vec4 outPrevMvpPosition;

void main() {
    Model models = bufferAddresses.modelBufferDeviceAddress.models[gl_InstanceIndex];
    mat4 currModel;
    mat4 prevModel;
    if(sceneData.frameNumber == 0){
        currModel = models.modelMatrix1;
        prevModel = models.modelMatrix2;
    } else {
        currModel = models.modelMatrix2;
        prevModel = models.modelMatrix1;
    }

    // Current
    vec4 worldPos = currModel * vec4(position, 1.0);

    outPosition = worldPos.xyz;
    outNormal = inverse(transpose(mat3(currModel))) * normal;
    outColor = color;
    outUV = uv;
    outMaterialIndex = materialIndex;
    outCurrMvpPosition = sceneData.viewProj * worldPos;
    outPrevMvpPosition = sceneData.prevViewProj * prevModel * vec4(position, 1.0);


    gl_Position = outCurrMvpPosition;
}