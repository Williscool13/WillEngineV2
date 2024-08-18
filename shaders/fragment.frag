#version 450

layout(location = 0) in vec2 TexCoord;
layout(location = 1) in vec3 Color;
layout(location = 0) out vec4 FragColor;

layout(set = 0, binding = 0) uniform sampler2D sourceImage;

vec3 linearTosRGB(vec3 color) {
    return pow(color, vec3(1.0 / 2.2));
}

void main() {
    //FragColor = texture(sourceImage, TexCoord);
    FragColor = vec4(linearTosRGB(Color), 1.0);
}