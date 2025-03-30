layout (push_constant) uniform PushConstants {
    vec2 cameraTanHalfFOV;

    vec2 ndcToViewMul;
    vec2 ndcToViewAdd;

    vec2 ndcToViewMul_x_PixelSize;

    float depthLinearizeMult;
    float depthLinearizeAdd;

    float effectRadius;
    float effectFalloffRange;
    float denoiseBlurBeta;

    float radiusMultiplier;
    float sampleDistributionPower;
    float thinOccluderCompensation;
    float finalValuePower;
    float depthMipSamplingOffset;
    uint noiseIndex;

    int debug;
} pushConstants;


// packing/unpacking for edges; 2 bits per edge mean 4 gradient values (0, 0.33, 0.66, 1) for smoother transitions!
float XeGTAO_PackEdges(vec4 edgesLRTB)
{
    // integer version:
    // edgesLRTB = saturate(edgesLRTB) * 2.9.xxxx + 0.5.xxxx;
    // return (((uint)edgesLRTB.x) << 6) + (((uint)edgesLRTB.y) << 4) + (((uint)edgesLRTB.z) << 2) + (((uint)edgesLRTB.w));
    //
    // optimized, should be same as above
    edgesLRTB = round(clamp(edgesLRTB, 0, 1) * 2.9);
    return dot(edgesLRTB, vec4(64.0 / 255.0, 16.0 / 255.0, 4.0 / 255.0, 1.0 / 255.0));
}

vec4 XeGTAO_UnpackEdges(float _packedVal)
{
    int packedVal = int(_packedVal * 255.5f);
    vec4 edgesLRTB;
    edgesLRTB.x = float((packedVal >> 6) & 0x03) / 3.0;// there's really no need for mask (as it's an 8 bit input) but I'll leave it in so it doesn't cause any trouble in the future
    edgesLRTB.y = float((packedVal >> 4) & 0x03) / 3.0;
    edgesLRTB.z = float((packedVal >> 2) & 0x03) / 3.0;
    edgesLRTB.w = float((packedVal >> 0) & 0x03) / 3.0;

    return clamp(edgesLRTB, 0, 1);
}