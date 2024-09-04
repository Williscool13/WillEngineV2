#version 450

layout (location = 0) in vec3 position;

layout (push_constant) uniform PushConstants {
    mat4 mvp;
} pushConstants;

void main() {
    gl_Position = pushConstants.mvp * vec4(position, 1.0);
}