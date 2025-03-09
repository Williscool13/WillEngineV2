#version 460
#extension GL_EXT_buffer_reference: require
#extension GL_EXT_nonuniform_qualifier: enable

#include "structure.glsl"
#include "shadows.glsl"
#include "lights.glsl"

layout (std140, set = 0, binding = 0) uniform ShadowCascadeData {
    CascadeSplit cascadeSplits[4];
    mat4 lightViewProj[4];
    DirectionalLight directionalLightData; // w is intensity
} shadowCascadeData;

layout (location = 0) in vec3 position;

layout (push_constant) uniform PushConstants {
    int cascadeIndex;
} push;

void main() {
    gl_Position = shadowCascadeData.lightViewProj[push.cascadeIndex] * vec4(position, 1.0);
}