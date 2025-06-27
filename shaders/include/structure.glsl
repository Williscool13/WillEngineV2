#extension GL_EXT_buffer_reference : require

struct Instance
{
    int modelIndex;
    int primitiveDataIndex;
    int bIsDrawn;
    uint padding0;
    uint padding1;
    uint padding2;
    uint padding3;
    uint padding4;
};

struct Model
{
    mat4 currentModelMatrix;
    mat4 previousModelMatrix;
    vec4 flags; // x: enabled, xyz: reserved for future use
};

struct MeshBounds
{
    float radius;
    vec3 position;
};

struct Primitive
{
    uint firstIndex;
    uint indexCount;
    int vertexOffset;
    uint bHasTransparent;
    uint materialIndex;
    uint padding0;
    uint padding1;
    uint padding2;
    vec4 boundingSphere;
};


struct Material
{
    vec4 colorFactor;
    vec4 metalRoughFactors; // x: metallic, y: roughness

    // Texture indices
    ivec4 textureImageIndices; // x: color, y: metallic-rough, z: normal, w: emissive
    ivec4 textureSamplerIndices; // x: color, y: metallic-rough, z: normal, w: emissive
    ivec4 textureImageIndices2; // x: occlusion, y: packed NRM, z: pad, w: pad
    ivec4 textureSamplerIndices2; // x: occlusion, y: packed NRM, z: pad, w: pad

    // UV transforms (offset.xy, scale.xy)
    vec4 colorUvTransform;
    vec4 metalRoughUvTransform;
    vec4 normalUvTransform;
    vec4 emissiveUvTransform;
    vec4 occlusionUvTransform;

    // Additional properties
    vec4 emissiveFactor; // xyz: color, w: strength
    vec4 alphaProperties; // x: cutoff, y: mode, z: doubleSided, w: unlit
    vec4 physicalProperties; // x: IOR, y: dispersion, z: normalScale, w: occlusionStrength
};

struct VkDrawIndexedIndirectCommand
{
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int  vertexOffset;
    uint firstInstance;
};

struct IndirectCount
{
    uint opaqueCount;
    uint transparentCount;
    // The maximum number of primitives that are in the buffer. Equal to size of indirect buffer
    uint limit;
    uint padding;
};

layout (buffer_reference, std430) readonly buffer Instances
{
    Instance instanceArray[];
};

layout (buffer_reference, std430) readonly buffer Models
{
    Model modelArray[];
};

layout (buffer_reference, std430) readonly buffer PrimitiveData
{
    Primitive primitiveArray[];
};

layout (buffer_reference, std430) readonly buffer Materials
{
    Material materialArray[];
};

layout(buffer_reference, std430) buffer CommandBuffer
{
    VkDrawIndexedIndirectCommand commandArray[];
};

layout(buffer_reference, std430) buffer DrawCounts
{
    IndirectCount indirectCount;
};
