#version 460

layout(location = 0) in vec3 inColor;

layout (location = 0) out vec4 albedoTarget;

void main() {
    // 0 indicates this won't do light calculations in deferred resolve
    albedoTarget = vec4(inColor, 0.0);
}
