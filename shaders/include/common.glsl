#ifndef COMMON_GLSL
#define COMMON_GLSL

mat3 adjugate(mat4 m) {
    return mat3(
    cross(m[1].xyz, m[2].xyz),
    cross(m[2].xyz, m[0].xyz),
    cross(m[0].xyz, m[1].xyz)
    );
}

vec3 aces(vec3 x) {
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec3 reinhardTonemap(vec3 v) {
    return v / (1.0f + v);
}

vec3 linear2srgb(vec3 linearRGB) {
    return mix(
    12.92 * linearRGB,
    1.055 * pow(linearRGB, vec3(1.0 / 2.4)) - 0.055,
    step(0.0031308, linearRGB)
    );
}

vec3 sRGB2Linear(vec3 sRGB) {
    return mix(
    sRGB / 12.92,
    pow((sRGB + 0.055) / 1.055, vec3(2.4)),
    step(0.04045, sRGB)
    );
}

float rgb2luma(vec3 rgb){
    const vec3 luma = vec3(0.299, 0.587, 0.114);
    return dot(rgb, luma);
}

#endif // COMMON_GLSL