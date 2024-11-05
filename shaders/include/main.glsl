#extension GL_EXT_buffer_reference : require

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


layout (std140, set = 0, binding = 0) uniform SceneData {
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