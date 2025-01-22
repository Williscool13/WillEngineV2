#version 450

layout(location = 0) in vec2 texCoord;
layout(location = 1) in vec3 color;
layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform sampler2D sourceImage;

void main() {
    //FragColor = texture(sourceImage, TexCoord);
    fragColor = vec4(color, 1.0);
}