#version 450

#include "scene.glsl"

layout(location = 0) in vec2 texCoord;
layout(location = 1) in vec3 color;
layout(location = 0) out vec4 fragColor;

// layout (std140, set = 0, binding = 0) uniform SceneData - scene.glsl

layout(set = 1, binding = 0) uniform sampler2D sourceImage;

layout(push_constant) uniform PushConstants {
    int frameNumber;
} pushConstants;

void main() {
    //FragColor = texture(sourceImage, TexCoord);
    fragColor = vec4(color, 1.0);
}