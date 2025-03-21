#version 450

layout(quads, equal_spacing, ccw) in;

void main() {
    float u = gl_TessCoord.x;
    float v = gl_TessCoord.y;

    vec4 pos0 = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, u);
    vec4 pos1 = mix(gl_in[2].gl_Position, gl_in[3].gl_Position, u);
    gl_Position = mix(pos0, pos1, v);
}