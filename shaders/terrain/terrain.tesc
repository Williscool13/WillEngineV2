#version 450

layout(vertices = 4) out;

layout(location = 0) in vec3 inPosition[];
layout(location = 1) in vec3 inNormal[];
layout(location = 2) in vec2 inTexCoord[];
layout(location = 3) in vec4 inColor[];
layout(location = 4) in vec4 inCurrMvpPosition[];
layout(location = 5) in vec4 inPrevMvpPosition[];

layout(location = 0) out vec3 outPosition[];
layout(location = 1) out vec3 outNormal[];
layout(location = 2) out vec2 outTexCoord[];
layout(location = 3) out vec4 outColor[];
layout(location = 4) out vec4 outCurrMvpPosition[];
layout(location = 5) out vec4 outPrevMvpPosition[];

layout(push_constant) uniform PushConstants {
    float tessLevel;
    //vec3 cameraPos; // for distance based LOD later on
} pushConstants;

void main() {
    outPosition[gl_InvocationID] = inPosition[gl_InvocationID];
    outNormal[gl_InvocationID] = inNormal[gl_InvocationID];
    outTexCoord[gl_InvocationID] = inTexCoord[gl_InvocationID];
    outColor[gl_InvocationID] = inColor[gl_InvocationID];
    outCurrMvpPosition[gl_InvocationID] = inCurrMvpPosition[gl_InvocationID];
    outPrevMvpPosition[gl_InvocationID] = inPrevMvpPosition[gl_InvocationID];

    if (gl_InvocationID == 0) {
        float tessLevel = pushConstants.tessLevel;

        gl_TessLevelOuter[0] = tessLevel;
        gl_TessLevelOuter[1] = tessLevel;
        gl_TessLevelOuter[2] = tessLevel;
        gl_TessLevelOuter[3] = tessLevel;

        gl_TessLevelInner[0] = tessLevel;
        gl_TessLevelInner[1] = tessLevel;
    }
}