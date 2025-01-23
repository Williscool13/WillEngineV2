#version 450

layout(location = 0) in vec2 texCoord;
layout(location = 1) in vec3 color;
layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform sampler2D sourceImage;

layout (std140, set = 1, binding = 0) uniform SceneData {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec4 cameraPos;
    mat4 viewProjCameraLookDirection;

    mat4 invView;
    mat4 invProjection;
    mat4 invViewProjection;

    mat4 prevView;
    mat4 prevProj;
    mat4 prevViewProj;

    vec4 jitter;

    vec2 renderTargetSize;
    float deltaTime;
    int currentFrame;
} sceneData;

layout(push_constant) uniform PushConstants {
    int frameNumber;
} pushConstants;

void main() {
    //FragColor = texture(sourceImage, TexCoord);
    fragColor = vec4(color, 1.0);
}