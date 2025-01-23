float linearizeReverseDepthRaw(float depth, float near, float far) {
    float zNear = far;
    float zFar = near;
    depth = 1 - depth;
    float z_n = 2.0 * depth - 1.0;
    return 2.0 * zNear * zFar / (zFar + zNear - z_n * (zFar - zNear));

}

float linearizeReverseDepth(float depth, float near, float far) {
    // normalizes to [0, 1]
    float rawDepth = linearizeReverseDepthRaw(depth, near, far);
    return (rawDepth - far) / (near - far);
}