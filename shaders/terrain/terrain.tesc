#version 450

layout(vertices = 4) out;

layout(location = 0) in vec3 inPosition[];
layout(location = 1) in vec3 inNormal[];
layout(location = 2) in vec2 inTexCoord[];
layout(location = 3) flat in int inMaterialIndex[];

layout(location = 0) out vec3 outPosition[];
layout(location = 1) out vec3 outNormal[];
layout(location = 2) out vec2 outTexCoord[];
layout(location = 3) flat out int outMaterialIndex[];

layout(push_constant) uniform PushConstants {
    float tessLevel;
    //vec3 cameraPos; // for distance based LOD later on
} pushConstants;

void main() {
    outPosition[gl_InvocationID] = inPosition[gl_InvocationID];
    outNormal[gl_InvocationID] = inNormal[gl_InvocationID];
    outTexCoord[gl_InvocationID] = inTexCoord[gl_InvocationID];
    outMaterialIndex[gl_InvocationID] = inMaterialIndex[gl_InvocationID];

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