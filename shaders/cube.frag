#version 450

layout(set = 1, binding = 0) uniform sampler samplers[32];
layout(set = 1, binding = 1) uniform texture2D textures[255];

// world space
layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec4 inColor;
layout (location = 3) in vec2 inUV;
layout (location = 4) flat in uint inMaterialIndex;

layout (location = 0) out vec4 FragColor;

void main() {
    FragColor = vec4(inNormal * 0.5 + 0.5f, 1.0);
}