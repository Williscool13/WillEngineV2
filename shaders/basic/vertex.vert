#version 450

layout(location = 0) out vec2 texCoord;
layout(location = 1) out vec3 color;

layout(push_constant) uniform PushConstants {
    int frameNumber;
} pushConstants;

void main() {
    vec2 positions[3] = vec2[](
    vec2(-0.75,  0.75),
    vec2( 0.0 , -0.75),
    vec2(0.75,   0.75)
    );

    vec2 pos = positions[gl_VertexIndex];

    texCoord = pos * 0.5 + 0.5;

    vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),  // Red
    vec3(0.0, 1.0, 0.0),  // Green
    vec3(0.0, 0.0, 1.0)   // Blue
    );
    // Smooth color transition
//    float t = fract(pushConstants.time);
//    int currentColorIndex = (gl_VertexIndex + int(pushConstants.time)) % 3;
//    int nextColorIndex = (currentColorIndex + 1) % 3;
//    color = mix(colors[currentColorIndex], colors[nextColorIndex], t);
    color = colors[gl_VertexIndex];

    gl_Position = vec4(pos, 1.0, 1.0);
}