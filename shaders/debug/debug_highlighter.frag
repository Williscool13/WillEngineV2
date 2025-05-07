#version 460

layout (location = 0) in vec3 inColor;

layout (location = 0) out vec4 imageTarget;

void main() {
    imageTarget = vec4(inColor, 1.0);
}
