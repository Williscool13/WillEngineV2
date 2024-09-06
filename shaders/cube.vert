#version 450

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec4 color;
layout (location = 3) in vec2 uv;
layout (location = 4) in uint materialIndex;

layout (location = 0) out vec3 outNormal;

layout (push_constant) uniform PushConstants {
    mat4 mvp;
} pushConstants;

void main() {
    gl_Position = pushConstants.mvp * vec4(position, 1.0);
    outNormal = normal;

    mat3 i_t_m = inverse(transpose(mat3(pushConstants.mvp)));
    //outNormal = i_t_m * normal;
}