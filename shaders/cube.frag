#version 450
layout (location = 0) in vec3 outNormal;

layout (location = 0) out vec4 FragColor;

void main() {
    FragColor = vec4(outNormal * 0.5 + 0.5f, 1.0);
}