#version 460

layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 outColor;

layout (set = 0, binding = 0) uniform sampler2D accumulationTexture;
layout (set = 0, binding = 1) uniform sampler2D revealageTexture;

const float EPSILON = 0.00001f;

void main() {
    float revealage = texture(revealageTexture, inUV).r;
    vec4 accum = texture(accumulationTexture, inUV);

    float maxComp = max(max(abs(accum.r), abs(accum.g)), abs(accum.b));

    // Suppress overflow
    if (isinf(maxComp)){
        accum.rgb = vec3(accum.a);
    }

    vec3 averageColor = accum.rgb / max(accum.a, EPSILON);

    outColor = vec4(averageColor, 1.0f - revealage);
    //outColor = vec4(averageColor, 0.2f);
}