#version 450

vec3 positions[3] = vec3[](
        vec3(-1.0, -1.0, 0.001), // Bottom-left corner
        vec3( 3.0, -1.0, 0.001), // Far right of the screen, which combined with the third vertex forms a full-screen quad
        vec3(-1.0,  3.0, 0.001)  // Far top of the screen, which combined with the other vertices forms a full-screen quad
);

layout (std140, set = 1, binding = 0) uniform SceneData {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    mat4 invView;
    mat4 invProjection;
    mat4 invViewProjection;
    mat4 viewProjCameraLookDirection;
    vec4 cameraPos;
} sceneData;

layout(location = 0) out vec3 fragPosition;
layout(location = 1) out vec2 uv;

void main() {
    vec3 pos = positions[gl_VertexIndex];
	gl_Position = vec4(pos, 1.0);
	fragPosition = (inverse(sceneData.viewProjCameraLookDirection) * vec4(pos, 1.0)).xyz;

    uv = vec2(pos.x, pos.y) * 0.5 + 0.5;
}