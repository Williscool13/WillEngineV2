#version 450

#include "main.glsl"

layout (location = 0) out vec3 fragPosition;
layout (location = 1) out vec2 uv;

void main() {
    vec3 vertices[3] = vec3[3](vec3(-1, -1, 0.001), vec3(3, -1, 0.001), vec3(-1, 3, 0.001));
    gl_Position = vec4(vertices[gl_VertexIndex], 1);
    fragPosition = (inverse(sceneData.viewProjCameraLookDirection) * gl_Position).xyz;
    uv = 0.5 * gl_Position.xy + vec2(0.5);
}