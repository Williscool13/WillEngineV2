#version 450

layout(vertices = 4) out;

layout(location = 0) in vec3 inPosition[];

layout (push_constant) uniform PushConstants {
    int cascadeIndex;
    int tessLevel;
//vec3 cameraPos; // for distance based LOD later on
} push;

void main() {
    gl_out[gl_InvocationID].gl_Position = vec4(inPosition[gl_InvocationID], 1.0);

    if (gl_InvocationID == 0) {
        float tessLevel = float(push.tessLevel);

        gl_TessLevelOuter[0] = tessLevel;
        gl_TessLevelOuter[1] = tessLevel;
        gl_TessLevelOuter[2] = tessLevel;
        gl_TessLevelOuter[3] = tessLevel;

        gl_TessLevelInner[0] = tessLevel;
        gl_TessLevelInner[1] = tessLevel;
    }
}