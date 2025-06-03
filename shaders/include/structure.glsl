#extension GL_EXT_buffer_reference : require


struct Primitive
{
    uint materialIndex;
    uint modelIndex;
    uint boundingVolumeIndex;
    uint bHasTransparent;
};

struct Model
{
    mat4 currentModelMatrix;
    mat4 previousModelMatrix;
    vec4 flags; // x: enabled, xyz: reserved for future use
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

layout (buffer_reference, std430) readonly buffer PrimitiveData
{
    Primitive primitives[];
};

layout (buffer_reference, std430) readonly buffer ModelData
{
    Model models[];
};

layout (buffer_reference, std430) readonly buffer MaterialData
{
    Material materials[];
};