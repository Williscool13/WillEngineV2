layout (std140, set = 0, binding = 0) uniform SceneData {
    mat4 view;
    mat4 proj;
    mat4 viewProj;

    mat4 invView;
    mat4 invProjection;
    mat4 invViewProjection;

    mat4 viewProjCameraLookDirection;

    mat4 prevView;
    mat4 prevProj;
    mat4 prevViewProj;

    mat4 prevInvView;
    mat4 prevInvProjection;
    mat4 prevInvViewProjection;

    mat4 prevViewProjCameraLookDirection;

    vec4 cameraPos;
    vec4 prevCameraPos;

    vec4 jitter;

    vec2 renderTargetSize;
    float deltaTime;
} sceneData;