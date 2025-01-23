vec3 RGBToYCoCg(vec3 rgb) {
    float Y = dot(rgb, vec3(0.25, 0.5, 0.25));
    float Co = dot(rgb, vec3(0.5, 0.0, -0.5));
    float Cg = dot(rgb, vec3(-0.25, 0.5, -0.25));
    return vec3(Y, Co, Cg);
}

vec3 YCoCgToRGB(vec3 YCoCg) {
    float Y = YCoCg.x;
    float Co = YCoCg.y;
    float Cg = YCoCg.z;
    return vec3(Y + Co - Cg, Y + Cg, Y - Co - Cg);
}