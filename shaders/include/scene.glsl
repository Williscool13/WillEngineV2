layout (std140, set = 0, binding = 0) uniform SceneData {
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
} sceneData;