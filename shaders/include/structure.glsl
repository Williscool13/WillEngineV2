#extension GL_EXT_buffer_reference : require

struct Model
{
    mat4 currentModelMatrix;
    mat4 previousModelMatrix;
    vec4 flags; // x: enabled, xyz: reserved for future use
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