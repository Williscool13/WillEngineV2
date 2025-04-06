#include "common.glsl"

vec3 FXAA_sampleTonemappedTexture(sampler2D tex, vec2 uv, bool tonemapEnabled){
    if (tonemapEnabled) {
        return aces(texture(tex, uv).rgb);
    }
    return texture(tex, uv).rgb;
}

float FXAA_Quality(int q) {
    if (q < 5) {
        return 1.0;
    } else if (q > 5) {
        if (q < 10) {
            return 2.0;
        } else if (q < 11) {
            return 4.0;
        } else {
            return 8.0;
        }
    } else {
        return 1.5;
    }
}

vec3 FXAA_Apply(vec3 centerColor, sampler2D tex, vec2 uv, vec2 texelSize, bool tonemapEnabled) {
    // Heavy Duty FXAA from https://github.com/kosua20/Rendu/blob/master/resources/common/shaders/screens/fxaa.frag
    const float FXAA_EDGE_THRESHOLD = 0.125;
    const float FXAA_EDGE_THRESHOLD_MIN = 0.0312;
    const float FXAA_SUBPIX_QUALITY = 0.75;

    // center is assumed to be in sRGB
    float lumaM = rgb2luma(linear2srgb(centerColor));
    float lumaB = rgb2luma(linear2srgb(FXAA_sampleTonemappedTexture(tex, uv + vec2(0, -texelSize.y), tonemapEnabled)));
    float lumaT = rgb2luma(linear2srgb(FXAA_sampleTonemappedTexture(tex, uv + vec2(0, texelSize.y), tonemapEnabled)));
    float lumaL = rgb2luma(linear2srgb(FXAA_sampleTonemappedTexture(tex, uv + vec2(-texelSize.x, 0), tonemapEnabled)));
    float lumaR = rgb2luma(linear2srgb(FXAA_sampleTonemappedTexture(tex, uv + vec2(texelSize.x, 0), tonemapEnabled)));

    float lumaMin = min(lumaM, min(min(lumaB, lumaT), min(lumaL, lumaR)));
    float lumaMax = max(lumaM, max(max(lumaB, lumaT), max(lumaL, lumaR)));

    float lumaRange = lumaMax - lumaMin;
    if (lumaRange < max(FXAA_EDGE_THRESHOLD_MIN, lumaMax * FXAA_EDGE_THRESHOLD)){
        return centerColor;
    }

    float lumaBL = rgb2luma(linear2srgb(FXAA_sampleTonemappedTexture(tex, uv + vec2(-texelSize.x, -texelSize.y), tonemapEnabled)));
    float lumaBR = rgb2luma(linear2srgb(FXAA_sampleTonemappedTexture(tex, uv + vec2(texelSize.x, -texelSize.y), tonemapEnabled)));
    float lumaTL = rgb2luma(linear2srgb(FXAA_sampleTonemappedTexture(tex, uv + vec2(-texelSize.x, texelSize.y), tonemapEnabled)));
    float lumaTR = rgb2luma(linear2srgb(FXAA_sampleTonemappedTexture(tex, uv + vec2(texelSize.x, texelSize.y), tonemapEnabled)));

    float lumaBT = lumaB + lumaT;
    float lumaLR = lumaL + lumaR;

    float lumaLeftCorners = lumaBL + lumaTL;
    float lumaDownCorners = lumaBL + lumaBR;
    float lumaRightCorners = lumaBR + lumaTR;
    float lumaUpCorners = lumaTL + lumaTR;

    float edgeHorizontal =    abs(-2.0 * lumaL + lumaLeftCorners)    + abs(-2.0 * lumaM + lumaBT) * 2.0    + abs(-2.0 * lumaR + lumaRightCorners);
    float edgeVertical =    abs(-2.0 * lumaT + lumaUpCorners)        + abs(-2.0 * lumaM + lumaLR) * 2.0    + abs(-2.0 * lumaB + lumaDownCorners);

    // Is the local edge horizontal or vertical ?
    bool isHorizontal = (edgeHorizontal >= edgeVertical);

    // Choose the step size (one pixel) accordingly.
    float stepLength = isHorizontal ? texelSize.y : texelSize.x;

    // Select the two neighboring texels lumas in the opposite direction to the local edge.
    float luma1 = isHorizontal ? lumaB : lumaL;
    float luma2 = isHorizontal ? lumaT : lumaR;
    // Compute gradients in this direction.
    float gradient1 = luma1 - lumaM;
    float gradient2 = luma2 - lumaM;

    // Which direction is the steepest ?
    bool is1Steepest = abs(gradient1) >= abs(gradient2);

    // Gradient in the corresponding direction, normalized.
    float gradientScaled = 0.25*max(abs(gradient1),abs(gradient2));

    // Average luma in the correct direction.
    float lumaLocalAverage = 0.0;
    if(is1Steepest){
        // Switch the direction
        stepLength = - stepLength;
        lumaLocalAverage = 0.5*(luma1 + lumaM);
    } else {
        lumaLocalAverage = 0.5*(luma2 + lumaM);
    }

    // Shift UV in the correct direction by half a pixel.
    vec2 currentUv = uv;
    if(isHorizontal){
        currentUv.y += stepLength * 0.5;
    } else {
        currentUv.x += stepLength * 0.5;
    }


    // Compute offset (for each iteration step) in the right direction.
    vec2 offset = isHorizontal ? vec2(texelSize.x,0.0) : vec2(0.0,texelSize.y);
    // Compute UVs to explore on each side of the edge, orthogonally. The QUALITY allows us to step faster.
    vec2 uv1 = currentUv - offset * FXAA_Quality(0);
    vec2 uv2 = currentUv + offset * FXAA_Quality(0);

    // Read the lumas at both current extremities of the exploration segment, and compute the delta wrt to the local average luma.
    float lumaEnd1 = rgb2luma(linear2srgb(FXAA_sampleTonemappedTexture(tex, uv1, tonemapEnabled)));
    float lumaEnd2 = rgb2luma(linear2srgb(FXAA_sampleTonemappedTexture(tex, uv2, tonemapEnabled)));
    lumaEnd1 -= lumaLocalAverage;
    lumaEnd2 -= lumaLocalAverage;

    // If the luma deltas at the current extremities is larger than the local gradient, we have reached the side of the edge.
    bool reached1 = abs(lumaEnd1) >= gradientScaled;
    bool reached2 = abs(lumaEnd2) >= gradientScaled;
    bool reachedBoth = reached1 && reached2;

    // If the side is not reached, we continue to explore in this direction.
    if(!reached1){
        uv1 -= offset * FXAA_Quality(0);
    }
    if(!reached2){
        uv2 += offset * FXAA_Quality(0);
    }

    // If both sides have not been reached, continue to explore.
    if(!reachedBoth){

        for(int i = 2; i < 12; i++){
            // If needed, read luma in 1st direction, compute delta.
            if(!reached1){
                lumaEnd1 = rgb2luma(linear2srgb(FXAA_sampleTonemappedTexture(tex, uv1, tonemapEnabled)));
                lumaEnd1 = lumaEnd1 - lumaLocalAverage;
            }
            // If needed, read luma in opposite direction, compute delta.
            if(!reached2){
                lumaEnd2 = rgb2luma(linear2srgb(FXAA_sampleTonemappedTexture(tex, uv2, tonemapEnabled)));
                lumaEnd2 = lumaEnd2 - lumaLocalAverage;
            }
            // If the luma deltas at the current extremities is larger than the local gradient, we have reached the side of the edge.
            reached1 = abs(lumaEnd1) >= gradientScaled;
            reached2 = abs(lumaEnd2) >= gradientScaled;
            reachedBoth = reached1 && reached2;

            // If the side is not reached, we continue to explore in this direction, with a variable quality.
            if(!reached1){
                uv1 -= offset * 1.0f;
            }
            if(!reached2){
                uv2 += offset * FXAA_Quality(i);
            }

            // If both sides have been reached, stop the exploration.
            if(reachedBoth){ break;}
        }

    }

    // Compute the distances to each side edge of the edge (!).
    float distance1 = isHorizontal ? (uv.x - uv1.x) : (uv.y - uv1.y);
    float distance2 = isHorizontal ? (uv2.x - uv.x) : (uv2.y - uv.y);

    // In which direction is the side of the edge closer ?
    bool isDirection1 = distance1 < distance2;
    float distanceFinal = min(distance1, distance2);

    // Thickness of the edge.
    float edgeThickness = (distance1 + distance2);

    // Is the luma at center smaller than the local average ?
    bool isLumaCenterSmaller = lumaM < lumaLocalAverage;

    // If the luma at center is smaller than at its neighbour, the delta luma at each end should be positive (same variation).
    bool correctVariation1 = (lumaEnd1 < 0.0) != isLumaCenterSmaller;
    bool correctVariation2 = (lumaEnd2 < 0.0) != isLumaCenterSmaller;

    // Only keep the result in the direction of the closer side of the edge.
    bool correctVariation = isDirection1 ? correctVariation1 : correctVariation2;

    // UV offset: read in the direction of the closest side of the edge.
    float pixelOffset = - distanceFinal / edgeThickness + 0.5;

    // If the luma variation is incorrect, do not offset.
    float finalOffset = correctVariation ? pixelOffset : 0.0;

    // Sub-pixel shifting
    // Full weighted average of the luma over the 3x3 neighborhood.
    float lumaAverage = (1.0/12.0) * (2.0 * (lumaBT + lumaLR) + lumaLeftCorners + lumaRightCorners);
    // Ratio of the delta between the global average and the center luma, over the luma range in the 3x3 neighborhood.
    float subPixelOffset1 = clamp(abs(lumaAverage - lumaM)/lumaRange,0.0,1.0);
    float subPixelOffset2 = (-2.0 * subPixelOffset1 + 3.0) * subPixelOffset1 * subPixelOffset1;
    // Compute a sub-pixel offset based on this delta.
    float subPixelOffsetFinal = subPixelOffset2 * subPixelOffset2 * 0.75f;

    // Pick the biggest of the two offsets.
    finalOffset = max(finalOffset,subPixelOffsetFinal);

    // Compute the final UV coordinates.
    vec2 finalUv = uv;
    if(isHorizontal){
        finalUv.y += finalOffset * stepLength;
    } else {
        finalUv.x += finalOffset * stepLength;
    }

    // Read the color at the new UV coordinates, and use it.
    return FXAA_sampleTonemappedTexture(tex, finalUv, tonemapEnabled);
}