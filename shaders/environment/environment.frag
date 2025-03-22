#version 450
layout (location = 0) in vec3 uv;
layout (location = 1) in vec4 inCurrMvpPosition;
layout (location = 2) in vec4 inPrevMvpPosition;

layout (location = 0) out vec4 normalTarget;// 10,10,10, (2 unused)
layout (location = 1) out vec4 albedoTarget;// 10,10,10, (2 -> 0 = environment Map, 1 = renderObject)
layout (location = 2) out vec4 pbrTarget;// 8 metallic, 8 roughness, 8 emissive (unused), 8 unused
layout (location = 3) out vec2 velocityTarget;// 16 X, 16 Y

layout(set = 1, binding = 0) uniform samplerCube environmentMap;

void main()
{
    vec3 direction = normalize(uv);
    direction.y = -direction.y;
    vec3 envColor = textureLod(environmentMap, direction, 0).rgb;

    // 0 = environment map flag
    albedoTarget = vec4(envColor, 0.0);

    normalTarget = vec4(-direction, 0.0f);
    pbrTarget = vec4(0.0f);

    vec2 currNdc = inCurrMvpPosition.xy / inCurrMvpPosition.w;
    vec2 prevNdc = inPrevMvpPosition.xy / inPrevMvpPosition.w;
    vec2 velocity = (currNdc - prevNdc) * 0.5f;
    velocityTarget = velocity;
}