float linearizeReverseDepthRaw(float depth, float near, float far) {
    // near should be a big number, far should be a small number
    depth = 1.0 - depth;
    float z = depth * 2.0 - 1.0;
    return (2.0 * far * near) / (near + far - z * (near - far));
}

float linearizeReverseDepth(float depth, float near, float far) {
    // normalizes to [0, 1]
    float rawDepth = linearizeReverseDepthRaw(depth, near, far);
    return (rawDepth - far) / (near - far);
}